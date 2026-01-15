import os
import sqlite3


def default_db_path():
    data_dir = _resolve_data_dir()
    return os.path.join(data_dir, "bz3web.db")


def _resolve_data_dir():
    from bz3web import config

    data_dir = config.get_config().get("data_dir", "data")
    base_dir = config.get_config_dir()
    if os.path.isabs(data_dir):
        return data_dir
    return os.path.normpath(os.path.join(base_dir, data_dir))


def init_db(db_path):
    os.makedirs(os.path.dirname(db_path), exist_ok=True)
    conn = sqlite3.connect(db_path)
    conn.execute("PRAGMA foreign_keys = ON")
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL COLLATE NOCASE UNIQUE,
            email TEXT NOT NULL COLLATE NOCASE UNIQUE,
            password_hash TEXT NOT NULL,
            password_salt TEXT NOT NULL,
            is_admin INTEGER NOT NULL DEFAULT 0,
            is_admin_manual INTEGER NOT NULL DEFAULT 0,
            is_locked INTEGER NOT NULL DEFAULT 0,
            locked_at TEXT,
            deleted INTEGER NOT NULL DEFAULT 0,
            deleted_at TEXT,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        )
        """
    )
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS servers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL COLLATE NOCASE UNIQUE,
            description TEXT,
            host TEXT NOT NULL,
            port INTEGER NOT NULL,
            max_players INTEGER,
            num_players INTEGER,
            owner_user_id INTEGER NOT NULL,
            last_heartbeat INTEGER,
            screenshot_id TEXT,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(owner_user_id) REFERENCES users(id) ON DELETE CASCADE
        )
        """
    )
    conn.execute("CREATE UNIQUE INDEX IF NOT EXISTS servers_host_port_unique ON servers(host, port)")
    duplicates = conn.execute(
        """
        SELECT host, port, COUNT(*) AS total
          FROM servers
         GROUP BY host, port
        HAVING total > 1
        LIMIT 1
        """
    ).fetchone()
    if duplicates:
        conn.close()
        raise ValueError(
            f"[bz3web] Error: duplicate server host+port found in database ({duplicates[0]}:{duplicates[1]})."
        )
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS user_admins (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            owner_user_id INTEGER NOT NULL,
            admin_user_id INTEGER NOT NULL,
            trust_admins INTEGER NOT NULL DEFAULT 0,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(owner_user_id, admin_user_id),
            FOREIGN KEY(owner_user_id) REFERENCES users(id) ON DELETE CASCADE,
            FOREIGN KEY(admin_user_id) REFERENCES users(id) ON DELETE CASCADE
        )
        """
    )
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS password_resets (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            token TEXT NOT NULL,
            expires_at INTEGER NOT NULL,
            FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE
        )
        """
    )
    conn.close()


def connect(db_path):
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA foreign_keys = ON")
    return conn


def add_server(conn, record):
    conn.execute(
        """
        INSERT INTO servers
            (name, description, host, port, max_players, num_players, owner_user_id,
             screenshot_id, last_heartbeat)
        VALUES
            (?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            record.get("name"),
            record.get("description"),
            record["host"],
            record["port"],
            record.get("max_players"),
            record.get("num_players"),
            record.get("owner_user_id"),
            record.get("screenshot_id"),
            record.get("last_heartbeat"),
        ),
    )
    conn.commit()


def update_server(conn, server_id, record):
    conn.execute(
        """
        UPDATE servers
        SET name = ?,
            description = ?,
            host = ?,
            port = ?,
            max_players = ?,
            num_players = ?,
            owner_user_id = ?,
            screenshot_id = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
        """,
        (
            record.get("name"),
            record.get("description"),
            record["host"],
            record["port"],
            record.get("max_players"),
            record.get("num_players"),
            record.get("owner_user_id"),
            record.get("screenshot_id"),
            server_id,
        ),
    )
    conn.commit()


def update_heartbeat(conn, server_id, timestamp, num_players=None, max_players=None):
    fields = ["last_heartbeat = ?", "updated_at = CURRENT_TIMESTAMP"]
    values = [timestamp]
    if num_players is not None:
        fields.insert(1, "num_players = ?")
        values.insert(1, num_players)
    if max_players is not None:
        fields.insert(1, "max_players = ?")
        values.insert(1, max_players)
    values.append(server_id)
    conn.execute(
        f"""
        UPDATE servers
        SET {", ".join(fields)}
        WHERE id = ?
        """,
        values,
    )
    conn.commit()


def update_server_port(conn, server_id, port):
    conn.execute(
        "UPDATE servers SET port = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
        (port, server_id),
    )
    conn.commit()


def delete_server(conn, server_id):
    conn.execute("DELETE FROM servers WHERE id = ?", (server_id,))
    conn.commit()


def list_servers(conn):
    return conn.execute(
        """
        SELECT servers.*,
               users.username AS owner_username
          FROM servers
          JOIN users ON users.id = servers.owner_user_id
         WHERE users.deleted = 0
         ORDER BY servers.created_at DESC
        """
    ).fetchall()


def get_server(conn, server_id):
    return conn.execute(
        """
        SELECT servers.*,
               users.username AS owner_username
          FROM servers
          JOIN users ON users.id = servers.owner_user_id
         WHERE servers.id = ?
        """,
        (server_id,),
    ).fetchone()


def get_server_by_name(conn, name):
    return conn.execute(
        """
        SELECT servers.*,
               users.username AS owner_username
          FROM servers
          JOIN users ON users.id = servers.owner_user_id
         WHERE servers.name = ?
           AND users.deleted = 0
        """,
        (name,),
    ).fetchone()


def get_server_by_host_port(conn, host, port):
    return conn.execute(
        """
        SELECT servers.*,
               users.username AS owner_username
          FROM servers
          JOIN users ON users.id = servers.owner_user_id
         WHERE servers.host = ?
           AND servers.port = ?
        """,
        (host, port),
    ).fetchone()


def list_ports_by_host(conn, host):
    rows = conn.execute("SELECT port FROM servers WHERE host = ? ORDER BY port", (host,)).fetchall()
    return [row["port"] for row in rows]


def list_user_servers(conn, user_id):
    return conn.execute(
        """
        SELECT servers.*,
               users.username AS owner_username
          FROM servers
          JOIN users ON users.id = servers.owner_user_id
         WHERE servers.owner_user_id = ?
           AND users.deleted = 0
         ORDER BY servers.created_at DESC
        """,
        (user_id,),
    ).fetchall()


def add_user(conn, username, email, password_hash, password_salt, is_admin=False, is_admin_manual=False):
    conn.execute(
        """
        INSERT INTO users (username, email, password_hash, password_salt, is_admin, is_admin_manual)
        VALUES (?, ?, ?, ?, ?, ?)
        """,
        (
            username,
            email,
            password_hash,
            password_salt,
            1 if is_admin else 0,
            1 if is_admin_manual else 0,
        ),
    )
    conn.commit()


def update_user_email(conn, user_id, email):
    conn.execute(
        "UPDATE users SET email = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
        (email, user_id),
    )
    conn.commit()


def update_user_username(conn, user_id, username):
    conn.execute(
        "UPDATE users SET username = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
        (username, user_id),
    )
    conn.commit()


def set_user_admin(conn, user_id, enabled):
    conn.execute(
        "UPDATE users SET is_admin = ?, is_admin_manual = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
        (1 if enabled else 0, 1 if enabled else 0, user_id),
    )
    conn.commit()


def set_user_admin_manual(conn, user_id, enabled):
    conn.execute(
        "UPDATE users SET is_admin_manual = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
        (1 if enabled else 0, user_id),
    )
    conn.commit()


def recompute_admin_flags(conn, root_admin_id):
    users = conn.execute(
        "SELECT id, deleted FROM users"
    ).fetchall()
    admin_ids = set()
    if root_admin_id:
        admin_ids.add(root_admin_id)
        direct_admins = conn.execute(
            "SELECT admin_user_id, trust_admins FROM user_admins WHERE owner_user_id = ?",
            (root_admin_id,),
        ).fetchall()
        direct_ids = [row["admin_user_id"] for row in direct_admins]
        admin_ids.update(direct_ids)
        trusted_ids = [row["admin_user_id"] for row in direct_admins if row["trust_admins"]]
        for trusted_id in trusted_ids:
            rows = conn.execute(
                "SELECT admin_user_id FROM user_admins WHERE owner_user_id = ?",
                (trusted_id,),
            ).fetchall()
            admin_ids.update(row["admin_user_id"] for row in rows)
    for row in users:
        if row["deleted"]:
            conn.execute("UPDATE users SET is_admin = 0 WHERE id = ?", (row["id"],))
        else:
            conn.execute(
                "UPDATE users SET is_admin = ? WHERE id = ?",
                (1 if row["id"] in admin_ids else 0, row["id"]),
            )
    conn.commit()


def set_user_password(conn, user_id, password_hash, password_salt):
    conn.execute(
        "UPDATE users SET password_hash = ?, password_salt = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
        (password_hash, password_salt, user_id),
    )
    conn.commit()


def set_user_locked(conn, user_id, locked, locked_at=None):
    if locked:
        if locked_at:
            conn.execute(
                """
                UPDATE users
                SET is_locked = 1,
                    locked_at = ?,
                    updated_at = CURRENT_TIMESTAMP
                WHERE id = ?
                """,
                (locked_at, user_id),
            )
        else:
            conn.execute(
                """
                UPDATE users
                SET is_locked = 1,
                    locked_at = CURRENT_TIMESTAMP,
                    updated_at = CURRENT_TIMESTAMP
                WHERE id = ?
                """,
                (user_id,),
            )
    else:
        conn.execute(
            """
            UPDATE users
            SET is_locked = 0,
                locked_at = NULL,
                updated_at = CURRENT_TIMESTAMP
            WHERE id = ?
            """,
            (user_id,),
        )
    conn.commit()


def set_user_deleted(conn, user_id, deleted, deleted_at=None):
    if deleted:
        if deleted_at:
            conn.execute(
                """
                UPDATE users
                SET deleted = 1,
                    deleted_at = ?,
                    updated_at = CURRENT_TIMESTAMP
                WHERE id = ?
                """,
                (deleted_at, user_id),
            )
        else:
            conn.execute(
                """
                UPDATE users
                SET deleted = 1,
                    deleted_at = CURRENT_TIMESTAMP,
                    updated_at = CURRENT_TIMESTAMP
                WHERE id = ?
                """,
                (user_id,),
            )
    else:
        conn.execute(
            """
            UPDATE users
            SET deleted = 0,
                deleted_at = NULL,
                updated_at = CURRENT_TIMESTAMP
            WHERE id = ?
            """,
            (user_id,),
        )
    conn.commit()


def delete_user(conn, user_id):
    conn.execute("DELETE FROM user_admins WHERE owner_user_id = ? OR admin_user_id = ?", (user_id, user_id))
    conn.execute("DELETE FROM users WHERE id = ?", (user_id,))
    conn.commit()


def get_user_by_email(conn, email):
    return conn.execute("SELECT * FROM users WHERE email = ?", (email,)).fetchone()


def get_user_by_id(conn, user_id):
    return conn.execute("SELECT * FROM users WHERE id = ?", (user_id,)).fetchone()


def get_user_by_username(conn, username):
    return conn.execute("SELECT * FROM users WHERE username = ?", (username,)).fetchone()


def list_users(conn):
    return conn.execute("SELECT * FROM users ORDER BY created_at DESC").fetchall()


def list_user_admins(conn, owner_user_id):
    rows = conn.execute(
        """
        SELECT users.username,
               users.id AS admin_user_id,
               user_admins.trust_admins
        FROM user_admins
        JOIN users ON users.id = user_admins.admin_user_id
        WHERE user_admins.owner_user_id = ?
          AND users.deleted = 0
          AND users.is_locked = 0
        ORDER BY users.username
        """,
        (owner_user_id,),
    ).fetchall()
    return [
        {
            "username": row["username"],
            "admin_user_id": row["admin_user_id"],
            "trust_admins": bool(row["trust_admins"]),
        }
        for row in rows
    ]


def add_user_admin(conn, owner_user_id, admin_user_id, trust_admins=False):
    conn.execute(
        """
        INSERT OR IGNORE INTO user_admins (owner_user_id, admin_user_id, trust_admins)
        VALUES (?, ?, ?)
        """,
        (owner_user_id, admin_user_id, 1 if trust_admins else 0),
    )
    conn.commit()


def remove_user_admin(conn, owner_user_id, admin_user_id):
    conn.execute(
        "DELETE FROM user_admins WHERE owner_user_id = ? AND admin_user_id = ?",
        (owner_user_id, admin_user_id),
    )
    conn.commit()


def set_user_admin_trust(conn, owner_user_id, admin_user_id, trust_admins):
    conn.execute(
        """
        UPDATE user_admins
        SET trust_admins = ?
        WHERE owner_user_id = ? AND admin_user_id = ?
        """,
        (1 if trust_admins else 0, owner_user_id, admin_user_id),
    )
    conn.commit()


def add_password_reset(conn, user_id, token, expires_at):
    conn.execute(
        "INSERT INTO password_resets (user_id, token, expires_at) VALUES (?, ?, ?)",
        (user_id, token, expires_at),
    )
    conn.commit()


def get_password_reset(conn, token):
    return conn.execute(
        "SELECT * FROM password_resets WHERE token = ?",
        (token,),
    ).fetchone()


def delete_password_reset(conn, token):
    conn.execute("DELETE FROM password_resets WHERE token = ?", (token,))
    conn.commit()
