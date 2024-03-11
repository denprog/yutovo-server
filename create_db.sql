SELECT 'CREATE DATABASE yutovo' WHERE NOT EXISTS (SELECT FROM pg_database WHERE datname = 'yutovo')\gexec

CREATE TABLE IF NOT EXISTS users
(
    user_id serial PRIMARY KEY,
    login varchar(20),
    password varchar(128),
    name text,
    email text,
    create_time timestamp with time zone not null default now(),
    document_id int default -1, --current document for this user
    language varchar(10),
    plan_id int not null default 1
);

CREATE TABLE IF NOT EXISTS user_sessions
(
    session_id uuid PRIMARY KEY,
    user_id int default -1,
    create_time timestamp with time zone not null default now(),
    expire_time bigint not null,
    document_id int default -1 --current document for this session
);

CREATE TABLE IF NOT EXISTS refresh_sessions
(
    user_id int,
    refresh_uuid uuid not null,
    create_time timestamp with time zone not null default now(),
    expire_time bigint not null
);

CREATE TABLE IF NOT EXISTS user_documents
(
    document_id serial PRIMARY KEY,
    user_id int not null, --onwer of the document
    name text,
    create_time timestamp with time zone not null default now(),
    change_time timestamp with time zone not null default now(),
    shared boolean default false,
    public boolean default true,
    document jsonb
);

CREATE TABLE IF NOT EXISTS user_logins
(
    user_id int,
    login_time timestamp with time zone,
    logout_time timestamp with time zone
);

CREATE TABLE IF NOT EXISTS user_plans
(
    plan_id serial PRIMARY KEY,
    max_files int not null,
    max_solving_time int not null --in seconds
);

INSERT INTO user_plans (plan_id, max_files, max_solving_time) VALUES (1, 10, 10)
