-- EXAMPLE 
-- TODO
CREATE TABLE sensor (
    uuid UUID PRIMARY KEY,
    name TEXT NOT NULL,
    type TEXT,
    sensor TEXT,
    apidetails TEXT,
    is_online BOOLEAN DEFAULT FALSE
);

CREATE TABLE IF NOT EXISTS measurements (
    id SERIAL PRIMARY KEY,
    schema_version INT,
    group_id TEXT,
    device_id TEXT NOT NULL,
    sensor TEXT NOT NULL,
    value DOUBLE PRECISION NOT NULL,
    unit TEXT,
    ts_ms BIGINT NOT NULL,
    seq INTEGER,
    topic TEXT,
    received_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
