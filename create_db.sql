SELECT 'CREATE DATABASE yutovo' WHERE NOT EXISTS (SELECT FROM pg_database WHERE datname = 'yutovo')\gexec

CREATE TABLE IF NOT EXISTS users
(
    id serial PRIMARY KEY,
    login varchar(20),
    password varchar(20),
    name text,
    email text,
    create_time timestamp with time zone,
    current_session uuid
);

CREATE TABLE IF NOT EXISTS sessions
(
    id uuid PRIMARY KEY,
    create_time timestamp with time zone,
    last_time timestamp with time zone,
    shared boolean,
    folder text
);

CREATE TABLE IF NOT EXISTS cookies
(
    cookie text PRIMARY KEY,
    user_id int,
    session_id uuid,
    life_time timestamp with time zone
);

CREATE TABLE IF NOT EXISTS user_sessions
(
    user_id int,
    session_id uuid,
    refresh_token uuid not null,
    expires bigint not null,
    create_time timestamp with time zone not null default now()
);

CREATE TABLE IF NOT EXISTS refresh_sessions
(
    user_id int,
    refresh_token uuid not null,
    expires bigint not null,
    create_time timestamp with time zone not null default now()
);

CREATE TABLE IF NOT EXISTS user_logins
(
    user_id int,
    login_time timestamp with time zone,
    logout_time timestamp with time zone
);