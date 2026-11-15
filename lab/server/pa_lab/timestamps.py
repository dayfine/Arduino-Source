"""The contract's UTC timestamps (`2026-11-14T06:10:12.124Z`) to and from epoch seconds."""

from __future__ import annotations

import calendar
import datetime
import time

FORMAT = '%Y-%m-%dT%H:%M:%S.%fZ'


def parse(text: str) -> float:
    moment = datetime.datetime.strptime(text, FORMAT)
    return calendar.timegm(moment.timetuple()) + moment.microsecond / 1e6


def format(epoch: float) -> str:
    millis = int(round(epoch * 1000))
    moment = datetime.datetime.utcfromtimestamp(millis // 1000)
    return moment.strftime('%Y-%m-%dT%H:%M:%S') + '.{:03d}Z'.format(millis % 1000)


def wall_clock() -> float:
    return time.time()
