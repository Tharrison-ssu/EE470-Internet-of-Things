
-- EE 470 - Database Assignment
-- Author: Taine Harrison
-- Description: MySQL database for storing fake IoT sensor data

-- ==========================================
-- Part 1 - Create Book Table
-- ==========================================

CREATE TABLE Book (
    id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(100) NOT NULL
);

-- Insert sample books
INSERT INTO Book (name)
VALUES
('MariaDB Book'),
('SQLDB Book');


-- ==========================================
-- Part 2 - Create Price Table
-- ==========================================

CREATE TABLE Price (
    id INT PRIMARY KEY,
    price DECIMAL(8,2) NOT NULL,
    FOREIGN KEY (id) REFERENCES Book(id)
);

-- Insert prices for each book
INSERT INTO Price (id, price)
VALUES
(1, 250.00),
(2, 225.00);

-- Display book names and prices together
SELECT Book.name, Price.price
FROM Book
JOIN Price ON Book.id = Price.id;


-- ==========================================
-- Part 3 - Create Sensor Register Table
-- ==========================================

CREATE TABLE sensor_register (
    node_name VARCHAR(10) PRIMARY KEY,
    manufacturer VARCHAR(10) NOT NULL,
    longitude DECIMAL(15,10) NOT NULL,
    latitude DECIMAL(15,10) NOT NULL
);

-- Register five different sensors
INSERT INTO sensor_register
(node_name, manufacturer, longitude, latitude)
VALUES
('node_1', 'Bosch', -122.6730, 38.3390),
('node_2', 'Sony', -122.6740, 38.3400),
('node_3', 'Samsung', -122.6750, 38.3410),
('node_4', 'Honeywell', -122.6760, 38.3420),
('node_5', 'Siemens', -122.6770, 38.3430);


-- ==========================================
-- Part 4 - Create Sensor Data Table
-- ==========================================

CREATE TABLE sensor_data (
    node_name VARCHAR(10) NOT NULL,
    time_received DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    temperature DECIMAL(6,2) NOT NULL,
    humidity DECIMAL(6,2) NOT NULL,

-- Prevent duplicate readings for the same sensor and time
    PRIMARY KEY (node_name, time_received),

-- Only accept data from registered sensors
    FOREIGN KEY (node_name)
        REFERENCES sensor_register(node_name),

-- Set acceptable temperature and humidity ranges
    CHECK (temperature BETWEEN -10 AND 100),
    CHECK (humidity BETWEEN 0 AND 100)
);


-- ==========================================
-- Part 5 - Insert Sensor Readings
-- ==========================================

-- Four readings per sensor at 30-minute intervals
INSERT INTO sensor_data
(node_name, time_received, temperature, humidity)
VALUES

-- Sensor 1
('node_1', '2022-10-01 11:00:00', 22.5, 45),
('node_1', '2022-10-01 11:30:00', 23.0, 47),
('node_1', '2022-10-01 12:00:00', 24.2, 46),
('node_1', '2022-10-01 12:30:00', 23.8, 48),

-- Sensor 2
('node_2', '2022-10-01 11:00:00', 25.0, 50),
('node_2', '2022-10-01 11:30:00', 25.5, 52),
('node_2', '2022-10-01 12:00:00', 26.0, 51),
('node_2', '2022-10-01 12:30:00', 25.8, 53),

-- Sensor 3
('node_3', '2022-10-01 11:00:00', 19.5, 60),
('node_3', '2022-10-01 11:30:00', 20.0, 62),
('node_3', '2022-10-01 12:00:00', 21.2, 61),
('node_3', '2022-10-01 12:30:00', 20.8, 63),

-- Sensor 4
('node_4', '2022-10-01 11:00:00', 28.0, 40),
('node_4', '2022-10-01 11:30:00', 28.5, 42),
('node_4', '2022-10-01 12:00:00', 29.0, 41),
('node_4', '2022-10-01 12:30:00', 29.5, 43),

-- Sensor 5
('node_5', '2022-10-01 11:00:00', 21.0, 55),
('node_5', '2022-10-01 11:30:00', 21.5, 57),
('node_5', '2022-10-01 12:00:00', 22.0, 56),
('node_5', '2022-10-01 12:30:00', 22.5, 58);


-- ==========================================
-- Part 6 - Create Combined Sensor VIEW
-- ==========================================

CREATE VIEW sensor_combined AS
SELECT
    sensor_data.node_name,
    sensor_register.manufacturer,
    sensor_register.longitude,
    sensor_register.latitude,
    sensor_data.time_received,
    sensor_data.temperature,
    sensor_data.humidity
FROM sensor_data
JOIN sensor_register
ON sensor_data.node_name = sensor_register.node_name;


-- ==========================================
-- Part 7 - Display Database Results
-- ==========================================

-- Display all registered sensors
SELECT * FROM sensor_register;

-- Display all sensor readings
SELECT * FROM sensor_data
ORDER BY node_name, time_received;

-- Display the combined sensor VIEW
SELECT * FROM sensor_combined
ORDER BY node_name, time_received;

-- Temperature must be between -10 and 100 Celsius
ALTER TABLE sensor_data
ADD CONSTRAINT check_temperature
CHECK (temperature BETWEEN -10 AND 100);

-- Humidity must be between 0 and 100 percent
ALTER TABLE sensor_data
ADD CONSTRAINT check_humidity
CHECK (humidity BETWEEN 0 AND 100);
