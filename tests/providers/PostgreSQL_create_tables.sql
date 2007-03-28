CREATE TABLE actor (
  actor_id serial PRIMARY KEY,
  first_name VARCHAR(45) NOT NULL,
  last_name VARCHAR(45) NOT NULL,
  last_update TIMESTAMP NOT NULL DEFAULT now ()
);

CREATE TABLE language (
  language_id serial NOT NULL PRIMARY KEY,
  name CHAR(20) NOT NULL,
  last_update TIMESTAMP NOT NULL DEFAULT now ()
);

CREATE DOMAIN "year" AS integer
        CONSTRAINT year_check CHECK (((VALUE >= 1901) AND (VALUE <= 2155)));

CREATE TABLE film (
  film_id serial NOT NULL PRIMARY KEY,
  title VARCHAR(255) NOT NULL,
  description TEXT DEFAULT NULL,
  release_year "year" DEFAULT NULL,
  language_id SMALLINT NOT NULL,
  original_language_id SMALLINT,
  rental_duration SMALLINT NOT NULL DEFAULT 3,
  rental_rate NUMERIC(4,2) NOT NULL DEFAULT 4.99,
  length SMALLINT,
  replacement_cost NUMERIC(5,2) NOT NULL DEFAULT 19.99,
  rating VARCHAR DEFAULT 'G',
  special_features TEXT[],
  last_update TIMESTAMP NOT NULL DEFAULT now (),
  CONSTRAINT film_rating_check CHECK ((((((rating = 'G'::text) OR (rating = 'PG'::text)) OR (rating = 'PG-13'::text)) OR (rating = 'R'::text)) OR (rating = 'NC-17'::text))),
  CONSTRAINT fk_film_language FOREIGN KEY (language_id) REFERENCES language (language_id) ON DELETE RESTRICT ON UPDATE CASCADE,
  CONSTRAINT fk_film_language_original FOREIGN KEY (original_language_id) REFERENCES language (language_id) ON DELETE RESTRICT ON UPDATE CASCADE
);

CREATE TABLE film_actor (
  actor_id INTEGER NOT NULL,
  film_id INTEGER NOT NULL,
  last_update TIMESTAMP NOT NULL DEFAULT now (),
  CONSTRAINT fk_film_actor_actor FOREIGN KEY (actor_id) REFERENCES actor (actor_id) ON DELETE RESTRICT ON UPDATE CASCADE,
  CONSTRAINT fk_film_actor_film FOREIGN KEY (film_id) REFERENCES film (film_id) ON DELETE RESTRICT ON UPDATE CASCADE
);
