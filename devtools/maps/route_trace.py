"""Reads the route walks of AutoStory runs from a SerialPrograms log, for both games, into one trace.

A trace is a list of runs (one per "Starting Program" line); a run is a list
of walks, in order; a walk has the plan(s) it walked by, the steps it took
and where they ended, and marks for what happened on the way. `to_json()`
gives the same thing as plain data, which is the format agents should read
(`trace.json` next to the pictures).

FireRed (PokemonFRLG_OverworldWalk.cpp, PokemonFRLG_OverworldRecovery.cpp):
    Route: plan, 3 steps: walk east 4 -> PalletTown (10, 6); walk north 4 (to wall) -> ...; face west -> ...
    Route: walk north 8 -> Route1 (12, 21)                  one per step, as it starts
    Leg: a wild battle after 1253ms, stopping the leg.      the step stopped part-way
    Leg: a wild battle after 3653ms, walking to the wall again.
    Route: stopped part-way, finding the player again at Route1 (12, 21); walk north 8 (to wall) -> Route1 (12, 21)
    Route: a message on the way to the warp.
The plan line is newer than most logs: without it a walk is the steps of one
story event, and its plan is those steps less the re-anchor holds and the
way back to the stopped step's end.

BDSP (PokemonBDSP_AutoStory_RouteWalk.cpp):
    [Route] <label>: plan from Route 202 (15, 25), Route 202 (16, 25):
        east 15 (to wall) to Route 202 (23, 25), Route 202 (30, 26)      (continuation lines)
    [Route] Leg 1/5: east 15 (to wall) to Route 202 (23, 25), ...
    [Walk] <label>: east 15 (to wall): blocked after 2800ms, 0 wild battle(s).
    [Route] <label>: west 8: stopped early after about 3 tiles, at Route 202 (25, 24); planning again.
    [Route] <label>: arrived at Route 202 (20, 25).
Positions are sets: a plan starts from every tile the player may be on.
"""
import re
from dataclasses import asdict, dataclass, field

from .route_log import places

DIRECTIONS = {'north': (0, -1), 'south': (0, 1), 'east': (1, 0), 'west': (-1, 0)}

PREFIX = re.compile(r'^\ufeff?(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d)\.\d+ - \[[^\]]*\]: (.*)$')
PROGRAM = re.compile(r'Starting Program: (\w+?):(\w+)')
CHECKPOINT = re.compile(r'^\[Checkpoint\] (\S+): (start|done|failed)(?:.*?: (.*))?')
EVENT = re.compile(r'^\[Event\] (\S+ \S+): (start|done|failed)(?:: (.*))?')
RELOAD = re.compile(r'reloading for attempt (\d+)|Soft reset completed|Loading the save')
FAILURE = re.compile(r'(?:OperationFailedException\w*|FatalProgramException|InternalProgramError): (.*)')

#  FRLG.
F_STEP = re.compile(r'(walk|surf|face) (north|south|east|west)(?: (\d+))?((?: \((?:warp|to wall)\))*) -> (\w+) \((-?\d+), (-?\d+)\)')
F_STEP_LINE = re.compile(r'^Route: ' + F_STEP.pattern + r'\s*$')
F_PLAN = re.compile(r'^Route: plan, \d+ steps?: (.*)$')
F_ANCHOR = re.compile(r'^Route: stopped part-way, finding the player again at (\w+) \((-?\d+), (-?\d+)\)(.*)$')
F_LEG = re.compile(r'^Leg: (.+?)( \(seen late\))?(?: after \d+ms,)? (stopping the leg|walking to the wall again|at the end of the leg)')
F_WARP_HOLD = re.compile(r'^Route: (.+) on the way to the warp\.')

#  BDSP.
B_PLAN = re.compile(r'^\[Route\] (.+?): plan from (.+?):\s*$')
B_PLAN_LEG = re.compile(r'^\s+(north|south|east|west) (\d+) \((to wall|open)\) to (.+?)\s*$')
B_LEG = re.compile(r'^\[Route\] Leg (\d+)/(\d+): (north|south|east|west) (\d+) \((to wall|open)\) to (.+?)\s*$')
B_WALK = re.compile(r'^\[Walk\] (.+?): (?:(?:north|south|east|west) \d+ \((?:to wall|open)\)(?:: [^:]+?)?: )?'
                    r'(blocked|limit|dialog|transition|arrived)(?: after (?:a )?\d+ms(?: hold)?)?, (\d+) wild battle')
B_STOPPED = re.compile(r'^\[Route\] (.+?): (?:north|south|east|west) \d+: (?:stopped early after about \d+ tiles|blocked after \d+ of \d+ tiles), at (.+?); planning again')
B_WAIT = re.compile(r'^\[Route\] (.+?): (?:north|south|east|west) \d+: (stopped after about \d+ tiles, short of the wall|blocked after \d+ of \d+ tiles); ')
B_ARRIVED = re.compile(r'^\[Route\] (.+?): arrived at (.+?)\.\s*$')
B_TRAINER = re.compile(r'^\[Route\] (.+?): (.+?) stopped us about (.+?)\.\s*$')
B_BATTLE = re.compile(r'^\[Walk\] (.+?): (wild battle.*|a battle started.*)$')


@dataclass
class Leg:
    action: str                 # walk, surf, face
    direction: str
    tiles: int
    to_wall: bool = False
    warp: bool = False
    ends: list = field(default_factory=list)    # [(map, x, y)]: the tiles it may end on

    def describe(self):
        if self.action == 'face':
            return f'face {self.direction}'
        flags = (' (warp)' if self.warp else '') + (' (to wall)' if self.to_wall else '')
        return f'{self.action} {self.direction} {self.tiles}{flags}'


@dataclass
class Plan:
    start: list                 # [(map, x, y)], empty when not known
    legs: list
    reason: str = 'plan'        # plan, replan (BDSP, from where it stopped)
    inferred_start: bool = False


@dataclass
class Step:
    leg: Leg
    role: str = 'plan'          # plan, hold (finding the player again), back (to the stopped leg's end)
    start: list = field(default_factory=list)   # where it started, if known
    end: list = field(default_factory=list)     # where it ended, if known
    outcome: str = ''           # done, stopped, blocked, limit, dialog, ...
    certain: bool = True        # False: where it went isn't known (a hold from an unknown tile)
    time: str = ''


@dataclass
class Mark:
    kind: str                   # battle, message, trainer, stop, reanchor, arrived, failed, wait
    at: list                    # [(map, x, y)]
    text: str
    step: int = -1              # index into Walk.steps, -1 if none


@dataclass
class Walk:
    label: str
    checkpoint: str = ''
    event: str = ''
    plans: list = field(default_factory=list)
    steps: list = field(default_factory=list)
    marks: list = field(default_factory=list)
    outcome: str = 'open'       # done, failed, open (the log ends, or nothing says)
    error: str = ''
    time: str = ''
    attempt: int = 1            # the checkpoint's attempt, after reloads


@dataclass
class Run:
    run_id: str
    game: str
    program: str
    time: str
    walks: list = field(default_factory=list)


def _leg_from_frlg(match):
    action, direction, tiles, flags, name, x, y = match.groups()
    return Leg(action, direction, int(tiles or 0), '(to wall)' in flags, '(warp)' in flags, [(name, int(x), int(y))])


def step_back(place, direction, tiles):
    dx, dy = DIRECTIONS[direction]
    return (place[0], place[1] - dx * tiles, place[2] - dy * tiles)


class _Parser:
    def __init__(self, maps=None):
        self.maps = maps or {}
        self.runs = []
        self.run = None
        self.checkpoint = ''
        self.event = ''
        self.walk = None
        self.pos = []           # where the player is, if known
        self.time = ''
        self.attempt = 1
        #  FRLG recovery.
        self.holds = []         # holds still to come, as logged in the re-anchor line
        self.back_to = None     # the stopped step's end, while walking back to it
        self.explicit_plan = False
        self.plan_pending = None    # BDSP: the plan whose continuation lines are being read

    #  Runs and walks.
    def start_run(self, game, program):
        self.close_walk()
        stamp = re.sub(r'[^0-9]', '', self.time)
        self.run = Run(f'{game.lower().replace("pokemon", "")}-{stamp[:8]}-{stamp[8:14]}', game, program, self.time)
        self.runs.append(self.run)
        self.checkpoint = self.event = ''
        self.pos = []
        self.attempt = 1

    def ensure_run(self, game):
        if self.run is None:
            self.start_run(game, 'unknown')
        elif self.run.game == '':
            self.run.game = game

    def new_walk(self, label):
        self.close_walk()
        self.walk = Walk(label, self.checkpoint, self.event, time=self.time, attempt=self.attempt)
        self.run.walks.append(self.walk)
        self.holds, self.back_to, self.explicit_plan = [], None, False
        return self.walk

    def close_walk(self, outcome=None):
        walk = self.walk
        if walk is None:
            return
        self.finish_step()
        if not self.explicit_plan and walk.steps and not walk.plans:
            self.implicit_plan(walk)
        if outcome and walk.outcome == 'open':
            walk.outcome = outcome
        self.walk = None
        self.plan_pending = None

    def finish_step(self):
        """The current step ended as planned unless something said otherwise."""
        if self.walk is None or not self.walk.steps:
            return
        step = self.walk.steps[-1]
        if step.outcome in ('', 'late', 'blocked', 'limit', 'arrived', 'transition'):
            if not step.outcome or step.outcome == 'late':
                step.outcome = 'done'
            if not step.end:
                step.end = list(step.leg.ends)
            self.pos = list(step.end)
        elif step.outcome == 'stopped' and not step.end:
            self.pos = []

    def mark(self, kind, at, text):
        if self.walk is None:
            return
        self.walk.marks.append(Mark(kind, list(at), text, len(self.walk.steps) - 1))

    def fail(self, text):
        walk = self.walk or (self.run.walks[-1] if self.run and self.run.walks else None)
        if walk is None or (walk is not self.walk and walk.event != self.event):
            return
        if walk.outcome == 'failed' and walk.error:
            return
        last = walk.steps[-1] if walk.steps else None
        if walk is self.walk and last is not None and last.outcome == '' and 'walk_overworld_route' in text:
            #  The walker failed on its step (e.g. no warp fade): it didn't end as planned.
            last.outcome = 'failed'
            self.pos = list(last.start)
        elif walk is self.walk:
            self.finish_step()
        at = self.pos or (last.start or last.leg.ends if last else [])
        walk.marks.append(Mark('failed', list(at), text, len(walk.steps) - 1))
        walk.outcome, walk.error = 'failed', text

    #  FRLG.
    def frlg_plan(self, text):
        self.ensure_run('PokemonFRLG')
        walk = self.new_walk(self.event or 'route')
        self.explicit_plan = True
        legs = [_leg_from_frlg(m) for m in F_STEP.finditer(text)]
        start = self.infer_start(legs[0]) if legs else []
        walk.plans.append(Plan(start, legs, 'plan', True))

    def infer_start(self, leg):
        """Where a walk's first step started: worked back from its end, or where the last walk ended."""
        if leg.action == 'face':
            return list(leg.ends)
        end = leg.ends[0]
        back = step_back(end, leg.direction, leg.tiles)
        known = self.maps.get(_key(end[0]))
        inside = back[1] >= 0 and back[2] >= 0 and (known is None or known.contains(back[1], back[2]))
        if not leg.warp and inside:
            return [back]
        return list(self.pos)

    def frlg_step(self, leg):
        self.ensure_run('PokemonFRLG')
        if self.walk is None:
            self.new_walk(self.event or 'route')
        walk = self.walk
        self.finish_step()
        if not walk.steps:
            planned = walk.plans[0].start if walk.plans else []
            self.pos = list(planned) or self.infer_start(leg)
        step = Step(leg, start=list(self.pos), time=self.time)
        last = walk.steps[-1] if walk.steps else None
        if self.holds:
            self.holds.pop(0)
            step.role = 'hold'
            step.certain = not self.holds       # only the last hold ends on a known tile
            if not step.certain:
                step.end = []
        elif self.back_to is not None:
            step.role = 'back'
            if leg.ends and leg.ends[0] == self.back_to:
                self.back_to = None
        elif last is not None and last.role == 'plan' and _same_leg(last.leg, leg) and leg.to_wall:
            step.role = 'again'     # a leg to a wall, walked again after a late interrupt
        walk.steps.append(step)
        if not step.certain:
            step.outcome = 'done'
            self.pos = []

    def frlg_anchor(self, at, rest):
        walk = self.walk
        if walk is None or not walk.steps:
            return
        stopped = walk.steps[-1]
        stopped.outcome = 'stopped'
        stopped.end = []
        self.pos = []
        self.holds = [_leg_from_frlg(m) for m in F_STEP.finditer(rest)]
        self.back_to = stopped.leg.ends[0] if stopped.leg.ends and stopped.leg.ends[0] != at else None
        self.mark('reanchor', [at], f'found again at {at[0]} ({at[1]}, {at[2]}) after {len(self.holds)} hold(s)')

    def frlg_leg_event(self, what, late, how):
        walk = self.walk
        if walk is None or not walk.steps:
            return
        step = walk.steps[-1]
        kind = 'battle' if 'battle' in what else 'message' if 'message' in what else 'interrupt'
        text = what + (' (seen late)' if late else '') + ', ' + how
        if how == 'stopping the leg':
            step.outcome = 'stopped'
            self.mark(kind, step.start, text)
        elif how == 'at the end of the leg':
            if late and step.leg.to_wall:
                step.outcome = 'late'
            self.mark(kind, step.leg.ends, text)
        else:
            self.mark(kind, step.start, text)

    def implicit_plan(self, walk):
        legs = [step.leg for step in walk.steps if step.role == 'plan']
        start = walk.steps[0].start if walk.steps else []
        walk.plans.append(Plan(list(start), legs, 'plan', True))

    #  BDSP.
    def bdsp_plan(self, label, start_text):
        self.ensure_run('PokemonBDSP')
        start = places(start_text)
        if self.walk is not None and self.walk.label == label and self.walk.outcome == 'open':
            self.finish_step()
            plan = Plan(start, [], 'replan')
        else:
            self.new_walk(label)
            self.explicit_plan = True
            plan = Plan(start, [], 'plan')
        self.walk.plans.append(plan)
        self.plan_pending = plan
        self.pos = list(start)

    def bdsp_leg(self, direction, tiles, to_wall, ends_text):
        if self.walk is None:
            self.ensure_run('PokemonBDSP')
            self.new_walk(self.event or 'route')
            self.explicit_plan = True
            self.walk.plans.append(Plan(list(self.pos), [], 'plan'))
        self.finish_step()
        leg = Leg('walk', direction, tiles, to_wall, False, places(ends_text))
        plan = self.walk.plans[-1] if self.walk.plans else None
        if plan is not None and not any(_same_leg(leg, l) for l in plan.legs):
            plan.legs.append(leg)
        role = 'plan' if plan is None or plan.reason == 'plan' else 'replan'
        self.walk.steps.append(Step(leg, role, list(self.pos), time=self.time))

    def bdsp_line(self, text):
        m = B_PLAN.match(text)
        if m:
            self.bdsp_plan(m.group(1), m.group(2))
            return
        self.plan_pending = None
        m = B_LEG.match(text)
        if m:
            self.bdsp_leg(m.group(3), int(m.group(4)), m.group(5) == 'to wall', m.group(6))
            return
        walk = self.walk
        if walk is None:
            return
        step = walk.steps[-1] if walk.steps else None
        m = B_WALK.match(text)
        if m and m.group(1) == walk.label and step is not None:
            how, battles = m.group(2), int(m.group(3))
            if how in ('blocked', 'limit', 'arrived', 'transition') and step.outcome != 'stopped':
                step.outcome = how
            if how == 'dialog':
                self.mark('message', step.start, 'a dialog stopped the leg')
            if battles:
                self.mark('battle', step.leg.ends, f'{battles} wild battle(s) on {step.leg.describe()}')
            return
        m = B_STOPPED.match(text)
        if m and m.group(1) == walk.label and step is not None:
            at = places(m.group(2))
            step.outcome, step.end = 'stopped', at
            self.pos = at
            self.mark('stop', at, text.split(': ', 1)[1])
            return
        m = B_WAIT.match(text)
        if m and m.group(1) == walk.label and step is not None:
            self.mark('wait', step.start, m.group(2))
            return
        m = B_ARRIVED.match(text)
        if m and m.group(1) == walk.label:
            at = places(m.group(2))
            if step is not None:
                if step.outcome in ('', 'blocked', 'limit'):
                    step.outcome = step.outcome or 'done'
                step.end = at
            self.pos = at
            self.mark('arrived', at, 'arrived')
            walk.outcome = 'done'
            return
        m = B_TRAINER.match(text)
        if m and m.group(1) == walk.label:
            self.mark('trainer', places(m.group(3)), f'{m.group(2)} stopped us')
            return
        m = B_BATTLE.match(text)
        if m and m.group(1) == walk.label:
            self.mark('battle', self.pos or (step.start if step else []), m.group(2))

    #  Lines.
    def feed(self, raw):
        m = PREFIX.match(raw)
        if not m:
            if self.plan_pending is not None:
                leg = B_PLAN_LEG.match(raw)
                if leg:
                    self.plan_pending.legs.append(Leg('walk', leg.group(1), int(leg.group(2)), leg.group(3) == 'to wall',
                                                      False, places(leg.group(4))))
            return
        self.time, text = m.group(1), m.group(2)
        p = PROGRAM.search(text)
        if p and '<b>' in text:
            self.start_run(p.group(1), f'{p.group(1)}:{p.group(2)}')
            return
        if self.run is None and not (text.startswith('Route: ') or text.startswith('[Route]')):
            return
        r = RELOAD.search(text)
        if r:
            #  The game reloaded a save: the last position no longer holds.
            self.close_walk()
            self.pos = []
            if r.group(1):
                self.attempt = int(r.group(1))
            return
        c = CHECKPOINT.match(text)
        if c:
            if c.group(2) == 'start':
                self.close_walk()
                self.checkpoint, self.event = c.group(1), ''
            elif c.group(2) == 'failed':
                self.fail(c.group(3) or text)
            else:
                self.attempt = 1
            return
        e = EVENT.match(text)
        if e:
            if e.group(2) == 'start':
                self.close_walk()
                self.event = e.group(1)
            elif e.group(2) == 'done':
                self.close_walk('done')
            else:
                self.fail(e.group(3) or 'failed')
                self.close_walk('failed')
            return
        f = FAILURE.search(text)
        if f:
            self.fail(f.group(1))
            return
        if text.startswith('Route: ') or text.startswith('Leg: '):
            self.frlg_line(text)
        elif text.startswith('[Route]') or text.startswith('[Walk]'):
            self.bdsp_line(text)

    def frlg_line(self, text):
        m = F_PLAN.match(text)
        if m:
            self.frlg_plan(m.group(1))
            return
        m = F_STEP_LINE.match(text)
        if m:
            self.frlg_step(_leg_from_frlg(m))
            return
        m = F_ANCHOR.match(text)
        if m:
            self.frlg_anchor((m.group(1), int(m.group(2)), int(m.group(3))), m.group(4))
            return
        m = F_LEG.match(text)
        if m:
            self.frlg_leg_event(m.group(1), bool(m.group(2)), m.group(3))
            return
        m = F_WARP_HOLD.match(text)
        if m and self.walk is not None:
            self.mark('message', self.walk.steps[-1].start if self.walk.steps else [], m.group(1) + ' on the way to the warp')


def _key(name):
    return re.sub(r'[^0-9a-z_]', '', name.lower())


def _same_leg(a, b):
    return a.direction == b.direction and a.tiles == b.tiles and a.ends == b.ends


def parse(lines, maps=None):
    """The runs in `lines` (a log's lines), each with its walks."""
    parser = _Parser(maps)
    for line in lines:
        parser.feed(line.rstrip('\r\n'))
    parser.close_walk()
    return [run for run in parser.runs if run.walks]


def to_json(runs):
    return [asdict(run) for run in runs]
