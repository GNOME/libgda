CREATE table "ACTOR" (
    "ACTOR_ID"    NUMBER NOT NULL,
    "FIRST_NAME"  VARCHAR2(45) NOT NULL,
    "LAST_NAME"   VARCHAR2(45) NOT NULL,
    "LAST_UPDATE" TIMESTAMP NOT NULL,
    constraint  "ACTOR_PK" primary key ("ACTOR_ID")
);

CREATE sequence "ACTOR_SEQ";

CREATE trigger "BI_ACTOR"  
  before insert on "ACTOR"              
  for each row 
begin  
    select "ACTOR_SEQ".nextval into :NEW.ACTOR_ID from dual;
end;

