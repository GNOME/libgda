# select host, user from mysql.user;

CREATE USER 'gdauser'@'%' IDENTIFIED BY 'PASSWORD';
GRANT ALL PRIVILEGES ON * . * TO 'gdauser'@'%';
CREATE USER 'gdauser'@'localhost' IDENTIFIED BY 'PASSWORD';
GRANT ALL PRIVILEGES ON * . * TO 'gdauser'@'localhost';
FLUSH PRIVILEGES;
