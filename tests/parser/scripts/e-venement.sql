--
-- PostgreSQL database dump
--

SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

--
-- Name: SCHEMA public; Type: COMMENT; Schema: -; Owner: -
--

COMMENT ON SCHEMA public IS 'Standard public schema';


SET search_path = public, pg_catalog;

CREATE LANGUAGE plpgsql;

--
-- Name: resume_tickets; Type: TYPE; Schema: public; Owner: -
--

CREATE TYPE resume_tickets AS (
	"transaction" bigint,
	manifid integer,
	nb bigint,
	tarif character varying,
	reduc integer,
	printed boolean,
	canceled boolean,
	prix numeric,
	prixspec numeric
);


--
-- Name: get_personneid(integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION get_personneid(integer) RETURNS bigint
    AS $_$SELECT personneid AS result FROM org_personne WHERE id = $1;$_$
    LANGUAGE sql STABLE STRICT;


--
-- Name: FUNCTION get_personneid(integer); Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON FUNCTION get_personneid(integer) IS 'retourne l''id d''une personne investie de la fonction $1
$1: org_personne.id';


--
-- Name: zeroifnull(bigint); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION zeroifnull(bigint) RETURNS bigint
    AS $_$BEGIN
IF $1 IS NULL THEN RETURN 0;
ELSE RETURN $1;
END IF;
END;$_$
    LANGUAGE plpgsql IMMUTABLE;


SET default_tablespace = '';

SET default_with_oids = true;

--
-- Name: entite; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE entite (
    id integer NOT NULL,
    nom character varying(127) NOT NULL,
    creation timestamp with time zone DEFAULT now() NOT NULL,
    modification timestamp with time zone DEFAULT now() NOT NULL,
    adresse text,
    cp character varying(10),
    ville character varying(255),
    pays character varying(255) DEFAULT 'France'::character varying,
    email character varying(255),
    npai boolean DEFAULT false NOT NULL,
    active boolean DEFAULT true NOT NULL
);


--
-- Name: TABLE entite; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON TABLE entite IS 'entités liées à l''organisme (personnes ou organismes)';


--
-- Name: COLUMN entite.cp; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN entite.cp IS 'code postal de l''adresse';


--
-- Name: COLUMN entite.email; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN entite.email IS 'adresse email';


--
-- Name: COLUMN entite.active; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN entite.active IS 'permet de "supprimer" une entité dans l''application tout en gardant sa trace...';


--
-- Name: entite_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE entite_id_seq
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;


--
-- Name: entite_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE entite_id_seq OWNED BY entite.id;


--
-- Name: fonction; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE fonction (
    id integer NOT NULL,
    libelle character varying(127) NOT NULL
);


--
-- Name: TABLE fonction; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON TABLE fonction IS 'Fonction liant une personne à un organisme (avec son intitulé exact par exemple)';


--
-- Name: COLUMN fonction.libelle; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN fonction.libelle IS 'intitulé type, servant dans les extractions par exemple';


--
-- Name: org_categorie; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE org_categorie (
    id integer NOT NULL,
    libelle character varying(255) NOT NULL
);


--
-- Name: TABLE org_categorie; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON TABLE org_categorie IS 'categories regroupant des sous catégories d''organismes';


--
-- Name: org_personne; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE org_personne (
    id integer NOT NULL,
    personneid bigint NOT NULL,
    organismeid bigint NOT NULL,
    fonction character varying(255),
    email character varying(255),
    service character varying(255),
    "type" integer,
    telephone character varying(40),
    description text
);


--
-- Name: TABLE org_personne; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON TABLE org_personne IS 'liaison entre des personnes et des organismes, au titre d''une fonction dans ledit organisme';


--
-- Name: COLUMN org_personne.personneid; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN org_personne.personneid IS 'personne.id';


--
-- Name: COLUMN org_personne.organismeid; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN org_personne.organismeid IS 'organisme.id';


--
-- Name: COLUMN org_personne.fonction; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN org_personne.fonction IS 'fonction au titre de laquelle une personne est liée à un organisme';


--
-- Name: COLUMN org_personne.email; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN org_personne.email IS 'email de la personne dans l''organisme';


--
-- Name: COLUMN org_personne.service; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN org_personne.service IS 'Service dans l''organisme où travaille la personne';


--
-- Name: COLUMN org_personne."type"; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN org_personne."type" IS 'fonction.id : type de fonction';


--
-- Name: COLUMN org_personne.telephone; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN org_personne.telephone IS 'téléphone professionel d''une personne liée à un organisme';


--
-- Name: COLUMN org_personne.description; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN org_personne.description IS 'description du pro';


--
-- Name: organisme; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE organisme (
    url character varying(255),
    categorie integer,
    description text
)
INHERITS (entite);


--
-- Name: TABLE organisme; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON TABLE organisme IS 'structures en contact avec l''organisme';


--
-- Name: COLUMN organisme.description; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN organisme.description IS 'Description de l''organisme';


--
-- Name: organisme_categorie; Type: VIEW; Schema: public; Owner: -
--

CREATE VIEW organisme_categorie AS
    SELECT organisme.id, organisme.nom, organisme.creation, organisme.modification, organisme.adresse, organisme.cp, organisme.ville, organisme.pays, organisme.email, organisme.npai, organisme.active, organisme.url, organisme.categorie, org_categorie.libelle AS catdesc, organisme.description FROM organisme, org_categorie WHERE (((organisme.categorie = org_categorie.id) AND (organisme.categorie IS NOT NULL)) AND (organisme.active = true)) UNION SELECT organisme.id, organisme.nom, organisme.creation, organisme.modification, organisme.adresse, organisme.cp, organisme.ville, organisme.pays, organisme.email, organisme.npai, organisme.active, organisme.url, NULL::"unknown" AS categorie, NULL::"unknown" AS catdesc, organisme.description FROM organisme WHERE ((organisme.categorie IS NULL) AND (organisme.active = true)) ORDER BY 14, 2;


--
-- Name: VIEW organisme_categorie; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON VIEW organisme_categorie IS 'Liste des organismes avec leur catégorie (qui est à NULL s''ils n''en ont pas)';


--
-- Name: personne; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE personne (
    prenom character varying(255),
    titre character varying(24)
)
INHERITS (entite);


--
-- Name: TABLE personne; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON TABLE personne IS 'contacts de l''organisme';


--
-- Name: personne_properso; Type: VIEW; Schema: public; Owner: -
--

CREATE VIEW personne_properso AS
    (((SELECT DISTINCT personne.id, personne.nom, personne.creation, personne.modification, personne.adresse, personne.cp, personne.ville, personne.pays, personne.email, personne.npai, personne.active, personne.prenom, personne.titre, organisme.id AS orgid, organisme.nom AS orgnom, organisme.categorie AS orgcat, organisme.adresse AS orgadr, organisme.cp AS orgcp, organisme.ville AS orgville, organisme.pays AS orgpays, organisme.email AS orgemail, organisme.url AS orgurl, organisme.description AS orgdesc, org_personne.service, org_personne.id AS fctorgid, fonction.id AS fctid, fonction.libelle AS fcttype, org_personne.fonction AS fctdesc, org_personne.email AS proemail, org_personne.telephone AS protel, organisme.catdesc AS orgcatdesc, org_personne.description FROM organisme_categorie organisme, personne, org_personne, fonction WHERE ((((personne.id = org_personne.personneid) AND (organisme.id = org_personne.organismeid)) AND (fonction.id = org_personne."type")) AND (org_personne."type" IS NOT NULL)) ORDER BY personne.id, personne.nom, personne.creation, personne.modification, personne.adresse, personne.cp, personne.ville, personne.pays, personne.email, personne.npai, personne.active, personne.prenom, personne.titre, organisme.id, organisme.nom, organisme.categorie, organisme.adresse, organisme.cp, organisme.ville, organisme.pays, organisme.email, organisme.url, organisme.description, org_personne.service, org_personne.id, fonction.id, fonction.libelle, org_personne.fonction, org_personne.email, org_personne.telephone, organisme.catdesc, org_personne.description) UNION (SELECT DISTINCT personne.id, personne.nom, personne.creation, personne.modification, personne.adresse, personne.cp, personne.ville, personne.pays, personne.email, personne.npai, personne.active, personne.prenom, personne.titre, organisme.id AS orgid, organisme.nom AS orgnom, organisme.categorie AS orgcat, organisme.adresse AS orgadr, organisme.cp AS orgcp, organisme.ville AS orgville, organisme.pays AS orgpays, organisme.email AS orgemail, organisme.url AS orgurl, organisme.description AS orgdesc, org_personne.service, org_personne.id AS fctorgid, NULL::integer AS fctid, NULL::text AS fcttype, org_personne.fonction AS fctdesc, org_personne.email AS proemail, org_personne.telephone AS protel, organisme.catdesc AS orgcatdesc, org_personne.description FROM organisme_categorie organisme, personne, org_personne WHERE (((personne.id = org_personne.personneid) AND (organisme.id = org_personne.organismeid)) AND (org_personne."type" IS NULL)) ORDER BY personne.id, personne.nom, personne.creation, personne.modification, personne.adresse, personne.cp, personne.ville, personne.pays, personne.email, personne.npai, personne.active, personne.prenom, personne.titre, organisme.id, organisme.nom, organisme.categorie, organisme.adresse, organisme.cp, organisme.ville, organisme.pays, organisme.email, organisme.url, organisme.description, org_personne.service, org_personne.id, 26, 27, org_personne.fonction, org_personne.email, org_personne.telephone, organisme.catdesc, org_personne.description)) UNION SELECT personne.id, personne.nom, personne.creation, personne.modification, personne.adresse, personne.cp, personne.ville, personne.pays, personne.email, personne.npai, personne.active, personne.prenom, personne.titre, NULL::"unknown" AS orgid, NULL::"unknown" AS orgnom, NULL::"unknown" AS orgcat, NULL::"unknown" AS orgadr, NULL::"unknown" AS orgcp, NULL::"unknown" AS orgville, NULL::"unknown" AS orgpays, NULL::"unknown" AS orgemail, NULL::"unknown" AS orgurl, NULL::"unknown" AS orgdesc, NULL::"unknown" AS service, NULL::"unknown" AS fctorgid, NULL::"unknown" AS fctid, NULL::"unknown" AS fcttype, NULL::"unknown" AS fctdesc, NULL::"unknown" AS proemail, NULL::"unknown" AS protel, NULL::"unknown" AS orgcatdesc, NULL::"unknown" AS description FROM personne) UNION SELECT NULL::"unknown" AS id, NULL::"unknown" AS nom, NULL::"unknown" AS creation, NULL::"unknown" AS modification, NULL::"unknown" AS adresse, NULL::"unknown" AS cp, NULL::"unknown" AS ville, NULL::"unknown" AS pays, NULL::"unknown" AS email, NULL::"unknown" AS npai, NULL::"unknown" AS active, NULL::"unknown" AS prenom, NULL::"unknown" AS titre, NULL::"unknown" AS orgid, NULL::"unknown" AS orgnom, NULL::"unknown" AS orgcat, NULL::"unknown" AS orgadr, NULL::"unknown" AS orgcp, NULL::"unknown" AS orgville, NULL::"unknown" AS orgpays, NULL::"unknown" AS orgemail, NULL::"unknown" AS orgurl, NULL::"unknown" AS orgdesc, NULL::"unknown" AS service, NULL::"unknown" AS fctorgid, NULL::"unknown" AS fctid, NULL::"unknown" AS fcttype, NULL::"unknown" AS fctdesc, NULL::"unknown" AS proemail, NULL::"unknown" AS protel, NULL::"unknown" AS orgcatdesc, NULL::"unknown" AS description ORDER BY 2, 12, 15, 27, 28, 24;


--
-- Name: VIEW personne_properso; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON VIEW personne_properso IS 'permet d''accéder à toutes les personnes de l''annuaire qu''elles soient pro ou non, qu''elles aient des fonctions au sein d''un organisme ou non';


--
-- Name: object; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE "object" (
    id bigint NOT NULL,
    name character varying(128) NOT NULL,
    description text
);


--
-- Name: TABLE "object"; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON TABLE "object" IS 'Base table for a unified scape for every objects';


--
-- Name: object_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE object_id_seq
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;


--
-- Name: object_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE object_id_seq OWNED BY "object".id;


--
-- Name: account; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE account (
    "login" character varying(32) NOT NULL,
    "password" character varying(32) NOT NULL,
    active boolean DEFAULT true NOT NULL,
    expire date,
    "level" integer DEFAULT 0 NOT NULL,
    email character varying(255)
)
INHERITS ("object");


--
-- Name: COLUMN account."level"; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN account."level" IS 'Niveau de droits octroyé... dépend de l''application. Ici >= 10 : admin ; >= 5 : possibilité de modifier des fiches ; < 5 : consultation simple';


--
-- Name: COLUMN account.email; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN account.email IS 'email de l''utilisateur';


SET default_with_oids = false;

--
-- Name: child; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE child (
    id integer NOT NULL,
    personneid integer NOT NULL,
    birth integer NOT NULL,
    name text
);


--
-- Name: TABLE child; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON TABLE child IS 'Permet de définir l''âge des enfants d''un contact';


--
-- Name: COLUMN child.personneid; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN child.personneid IS 'personne.id';


--
-- Name: COLUMN child.birth; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN child.birth IS 'year of birth';


--
-- Name: COLUMN child.name; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN child.name IS 'child''s name';


--
-- Name: child_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE child_id_seq
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;


--
-- Name: child_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE child_id_seq OWNED BY child.id;


--
-- Name: fonction_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE fonction_id_seq
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;


--
-- Name: fonction_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE fonction_id_seq OWNED BY fonction.id;


--
-- Name: groupe; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE groupe (
    id integer NOT NULL,
    nom character varying(255) NOT NULL,
    createur bigint,
    creation timestamp with time zone DEFAULT now() NOT NULL,
    modification timestamp with time zone DEFAULT now() NOT NULL,
    description text
);


--
-- Name: TABLE groupe; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON TABLE groupe IS 'groupes de personnes créés à partir du requêteur';


--
-- Name: COLUMN groupe.id; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe.id IS 'id du groupe permettant de reconsituer le nom système de la view représentant le groupe ("grp_`id`")';


--
-- Name: COLUMN groupe.nom; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe.nom IS 'nom usuel du groupe';


--
-- Name: COLUMN groupe.createur; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe.createur IS 'lien vers le createur du groupe (account.id)';


--
-- Name: groupe_andreq; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE groupe_andreq (
    id integer NOT NULL,
    fctid integer,
    orgid integer,
    orgcat integer,
    cp character varying(10),
    ville character varying(255),
    npai boolean DEFAULT false,
    email boolean DEFAULT false,
    adresse boolean DEFAULT false,
    infcreation date,
    infmodification date,
    supcreation date,
    supmodification date,
    groupid integer NOT NULL,
    grpinc integer[],
    childmax integer,
    childmin integer
);


--
-- Name: TABLE groupe_andreq; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON TABLE groupe_andreq IS 'chaque ligne correspond à un groupe de ET logiques qui, regroupées en OU logiques, définissent un groupe...';


--
-- Name: COLUMN groupe_andreq.fctid; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe_andreq.fctid IS 'fonction.id';


--
-- Name: COLUMN groupe_andreq.orgid; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe_andreq.orgid IS 'organisme.id';


--
-- Name: COLUMN groupe_andreq.orgcat; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe_andreq.orgcat IS 'org_categorie.id';


--
-- Name: COLUMN groupe_andreq.cp; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe_andreq.cp IS 'personne.cp LIKE ''cp%'' OR organisme.cp LIKE ''cp%''';


--
-- Name: COLUMN groupe_andreq.ville; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe_andreq.ville IS 'personne.ville LIKE ''ville%'' OR organisme.ville LIKE ''ville%''';


--
-- Name: COLUMN groupe_andreq.npai; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe_andreq.npai IS 'personne.npai';


--
-- Name: COLUMN groupe_andreq.email; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe_andreq.email IS 'personne.email IS NULL => true (si une personne N''a PAS d''email)';


--
-- Name: COLUMN groupe_andreq.adresse; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe_andreq.adresse IS 'personne.adresse IS NULL => true (une personne N''a PAS d''adresse)';


--
-- Name: COLUMN groupe_andreq.infcreation; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe_andreq.infcreation IS 'personne.creation < infcreation';


--
-- Name: COLUMN groupe_andreq.infmodification; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe_andreq.infmodification IS 'personne.modification < infmodification';


--
-- Name: COLUMN groupe_andreq.supcreation; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe_andreq.supcreation IS 'personne.creation >= supcreation';


--
-- Name: COLUMN groupe_andreq.supmodification; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe_andreq.supmodification IS 'personne.modification >= supmodification';


--
-- Name: COLUMN groupe_andreq.groupid; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe_andreq.groupid IS 'groupe.id';


--
-- Name: COLUMN groupe_andreq.grpinc; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe_andreq.grpinc IS 'inclusion de groupes dans la condition';


--
-- Name: COLUMN groupe_andreq.childmax; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe_andreq.childmax IS 'date("Y") - childmax >= child.birth';


--
-- Name: COLUMN groupe_andreq.childmin; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe_andreq.childmin IS 'date("Y") - childmin <= child.birth';


--
-- Name: groupe_andreq_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE groupe_andreq_id_seq
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;


--
-- Name: groupe_andreq_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE groupe_andreq_id_seq OWNED BY groupe_andreq.id;


--
-- Name: groupe_fonctions; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE groupe_fonctions (
    groupid integer NOT NULL,
    fonctionid integer NOT NULL,
    included boolean DEFAULT false NOT NULL,
    info text
);


--
-- Name: TABLE groupe_fonctions; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON TABLE groupe_fonctions IS 'Liaison directe entre fonctions au sein d''un organisme et groupe... une fonction est liée à un groupe avec un booléen qui exprime si elle est exclue (false) ou inclue (true).';


--
-- Name: COLUMN groupe_fonctions.groupid; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe_fonctions.groupid IS 'groupe.id';


--
-- Name: COLUMN groupe_fonctions.fonctionid; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe_fonctions.fonctionid IS 'org_personne.id';


--
-- Name: COLUMN groupe_fonctions.info; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe_fonctions.info IS 'Colonne permettant de stocker des informations subsidiaires';


--
-- Name: groupe_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE groupe_id_seq
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;


--
-- Name: groupe_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE groupe_id_seq OWNED BY groupe.id;


--
-- Name: groupe_personnes; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE groupe_personnes (
    groupid integer NOT NULL,
    personneid integer NOT NULL,
    included boolean DEFAULT false NOT NULL,
    info text
);


--
-- Name: TABLE groupe_personnes; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON TABLE groupe_personnes IS 'Liaison directe entre personnes et groupe... une personne est liée à un groupe avec un booléen qui exprime si elle est exclue (false) ou inclue (true).';


--
-- Name: COLUMN groupe_personnes.groupid; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe_personnes.groupid IS 'groupe.id';


--
-- Name: COLUMN groupe_personnes.included; Type: COMMENT; Schema: public; Owner: -
--

COMMENT ON COLUMN groupe_personnes.included IS 'la personne est incluse dans le groupe ? (si non : elle est exclue)';

CREATE FUNCTION addpreresa(bigint, bigint, integer, integer, boolean, character varying, integer) RETURNS boolean
    AS $_$
DECLARE
account ALIAS FOR $1;
transac ALIAS FOR $2;
manif ALIAS FOR $3;
reduction ALIAS FOR $4;
annulation ALIAS FOR $5;
tarif ALIAS FOR $6;
nbloops ALIAS FOR $7;
nb integer;
tarif_id integer;

BEGIN

nb := 0;

tarif_id := get_tarifid(manif,tarif);

WHILE nb < ABS(nbloops) LOOP
  nb := nb + 1;
  INSERT INTO reservation_pre ("accountid","manifid","tarifid","reduc","transaction","annul")
  VALUES ( account, manif,tarif_id,reduction,transac,annulation );
END LOOP;

RETURN nb > 0;
END;$_$
    LANGUAGE plpgsql;


CREATE FUNCTION contingeanting(bigint, bigint, bigint, bigint) RETURNS boolean
    AS $_$BEGIN
PERFORM * FROM contingeant WHERE transaction = $1;
IF ( FOUND )
THEN RETURN false;
ELSE INSERT INTO contingeant (transaction,accountid,personneid,fctorgid) VALUES ($1,$2,$3,$4);
     RETURN true;
END IF;
END;$_$
    LANGUAGE plpgsql;


COMMENT ON FUNCTION contingeanting(bigint, bigint, bigint, bigint) IS 'fonction permettant d''ajouter _au_besoin_ une entrée dans la table contingeant.
retourne true si aucun enregistrement n''existait avant l''appel à la fonction (qui en a alors rajouté un),
retourne false sinon.
$1: transaction
$2: accountid
$3: personneid
$4: fctorgid';


CREATE FUNCTION counttickets(bigint, boolean) RETURNS bigint
    AS $_$SELECT count(*) AS RESULT
FROM reservation_cur AS resa
WHERE resa.canceled = false
AND resa_preid = $1;$_$
    LANGUAGE sql STABLE STRICT;


-- BIG one...
CREATE FUNCTION decontingeanting(bigint, integer, bigint, integer, integer, integer, integer) RETURNS boolean
    AS $_$DECLARE
trans ALIAS FOR $1;
manif ALIAS FOR $2;
account ALIAS FOR $3;
oldtarif ALIAS FOR $4;
newtarif ALIAS FOR $5;
reduction ALIAS FOR $6;
qty ALIAS FOR $7;

i INTEGER := 0;
selled INTEGER := 0;
mass RECORD;
BEGIN

-- calcul du nombre de places vendues
selled := (SELECT nb FROM masstickets WHERE tarifid = newtarif AND manifid = manif AND transaction = trans) - qty;

-- Si on a rien vendu, on ne met rien à jour
IF ( selled <= 0 ) THEN RETURN true; END IF;

-- Mise à jour de la table masstickets (on doit avoir qqch à mettre à jour)
UPDATE masstickets SET nb = qty WHERE tarifid = newtarif AND manifid = manif AND transaction = trans;
IF ( NOT FOUND ) THEN RETURN false; END IF;

LOOP
-- condition de sortie de boucle
IF ( i >= selled ) THEN RETURN true; END IF;

-- Si on n'a pas de pré-resa en attente... on en ajoute à la volée
PERFORM * FROM reservation_pre AS resa WHERE transaction = trans AND manifid = manif AND tarifid = oldtarif;
IF ( NOT FOUND )
THEN
  INSERT INTO reservation_pre (transaction,accountid,manifid,tarifid,reduc) SELECT trans, account, manif, oldtarif, 0;
  IF ( NOT FOUND )
  THEN RETURN false;
  END IF;
END IF;

-- On passe les pré-resa en résa réelle (puisque les tickets ont été vendus)
INSERT INTO reservation_cur (resa_preid,accountid)
VALUES ((SELECT MIN(id) AS resa_preid
         FROM reservation_pre AS resa
         WHERE transaction = trans AND manifid = manif AND account != 0 AND tarifid = oldtarif), account);
IF ( NOT FOUND ) THEN RETURN false; END IF;

-- On met à jour la nature des tarifs (on doit avoir qqch à mettre à jour)
UPDATE reservation_pre
SET tarifid = newtarif, reduc = reduction
WHERE id = (SELECT MIN(id) AS min FROM reservation_pre AS resa WHERE transaction = trans AND tarifid = oldtarif AND manifid = manif);
IF ( NOT FOUND ) THEN RETURN false; END IF;

i := i+1;

END LOOP;
RETURN true;
END;$_$
    LANGUAGE plpgsql STRICT;


CREATE FUNCTION getprice(integer, character varying) RETURNS numeric
    AS $_$DECLARE
    buf NUMERIC;
BEGIN
    
    buf := (    SELECT prix
                FROM manifestation_tarifs
                WHERE manifestationid = $1
                  AND tarifid = get_tarifid($1,$2));
    IF ( buf IS NOT NULL )
    THEN RETURN buf;
    END IF;
    
    buf := (    SELECT prix
                FROM tarif
                WHERE id = get_tarifid($1,$2));
    RETURN buf;
END;$_$
    LANGUAGE plpgsql STABLE STRICT;
