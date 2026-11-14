"""The control database file: migrations, version guard and permissions (Slice 3, fork #131)."""

import os
import sqlite3
import stat
import unittest

from pa_lab import db
from tests.control_fixtures import temp_db_path


class MigrationTests(unittest.TestCase):
    def test_a_new_file_is_migrated_to_the_current_version(self):
        database = db.Database(temp_db_path(self))
        self.addCleanup(database.close)
        conn = database.read()
        tables = {row[0] for row in conn.execute("SELECT name FROM sqlite_master WHERE type = 'table'")}
        self.assertEqual(conn.execute('PRAGMA user_version').fetchone()[0], db.SCHEMA_VERSION)
        self.assertTrue({'leases', 'requests', 'events', 'cursors', 'observations', 'meta'} <= tables)
        self.assertEqual(database.found_version, 0)

    def test_reopening_a_migrated_file_changes_nothing(self):
        path = temp_db_path(self)
        db.Database(path).close()
        again = db.Database(path)
        self.addCleanup(again.close)
        self.assertEqual(again.found_version, db.SCHEMA_VERSION)

    def test_a_file_from_a_newer_daemon_is_refused(self):
        path = temp_db_path(self)
        db.Database(path).close()
        conn = sqlite3.connect(str(path))
        conn.execute('PRAGMA user_version = {}'.format(db.SCHEMA_VERSION + 1))
        conn.commit()
        conn.close()
        with self.assertRaisesRegex(db.DatabaseError, 'schema {}'.format(db.SCHEMA_VERSION + 1)):
            db.Database(path)

    def test_the_file_is_owner_only(self):
        path = temp_db_path(self)
        db.Database(path).close()
        self.assertEqual(stat.S_IMODE(os.stat(path).st_mode) & 0o077, 0)

    def test_a_failed_write_leaves_nothing(self):
        database = db.Database(temp_db_path(self))
        self.addCleanup(database.close)
        with self.assertRaises(RuntimeError):
            with database.write() as conn:
                conn.execute("INSERT INTO meta (key, value) VALUES ('k', 'v')")
                raise RuntimeError('crash between statements')
        self.assertIsNone(database.read().execute("SELECT value FROM meta WHERE key = 'k'").fetchone())


if __name__ == '__main__':
    unittest.main()
