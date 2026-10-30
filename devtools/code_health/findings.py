"""Located findings and their GitHub Actions workflow-command form."""
from dataclasses import dataclass

# GitHub shows at most 10 of each level per step in the diff; more is noise in the log.
ANNOTATION_CAP = 50
TITLES = {'error': 'Code health', 'warning': 'Code health (advisory)'}


@dataclass(frozen=True)
class Finding:
    path: str
    line: int
    message: str

    def __str__(self):
        return f'{self.path}:{self.line}: {self.message}'


def escape_data(text):
    return text.replace('%', '%25').replace('\r', '%0D').replace('\n', '%0A')


def escape_property(text):
    return escape_data(str(text)).replace(':', '%3A').replace(',', '%2C')


def annotation(level, finding):
    properties = ','.join(f'{key}={escape_property(value)}' for key, value in
                          (('file', finding.path), ('line', finding.line), ('title', TITLES[level])))
    return f'::{level} {properties}::{escape_data(finding.message)}'


def annotations(level, findings, cap=ANNOTATION_CAP):
    """Workflow commands for at most `cap` findings, then a plain note about the rest."""
    lines = [annotation(level, finding) for finding in findings[:cap]]
    if len(findings) > cap:
        lines.append(f'Annotated {cap} of {len(findings)} {TITLES[level].lower()} findings; '
                     'see the log above for the rest.')
    return lines
