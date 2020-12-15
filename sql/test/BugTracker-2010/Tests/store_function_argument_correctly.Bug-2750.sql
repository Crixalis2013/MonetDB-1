START TRANSACTION;

create function f ( x varchar(20) ) returns varchar(10) begin return x; end;

select f.name, a.name, a."type", a.type_digits from functions f, args a where a.func_id = f.id and f.name = 'f';

CREATE TABLE branches (
  bid int NOT NULL default '0',
  cid tinyint NOT NULL default '0',
  bdesc varchar(255) NOT NULL default '',
  bloc char(3) NOT NULL default '');

INSERT INTO branches (bid, cid, bdesc, bloc) VALUES
 (1011, 101, 'Corporate HQ', 'CA'),
 (1012, 101, 'Accounting Department', 'NY'),
 (1013, 101, 'Customer Grievances Department', 'KA'),
 (1041, 104, 'Branch Office (East)', 'MA'),
 (1042, 104, 'Branch Office (West)', 'CA'),
 (1101, 110, 'Head Office', 'CA'),
 (1031, 103, 'N Region HO', 'ME'),
 (1032, 103, 'NE Region HO', 'CT'),
 (1033, 103, 'NW Region HO', 'NY');

CREATE TABLE clients (
  cid tinyint NOT NULL default '0',
  cname varchar(255) NOT NULL default '',
  PRIMARY KEY (cid));

INSERT INTO clients (cid, cname) VALUES
 (101, 'JV Real Estate'),
 (102, 'ABC Talent Agency'),
 (103, 'DMW Trading'),
 (104, 'Rabbit Foods Inc'),
 (110, 'Sharp Eyes Detective Agency');

CREATE function client_id(cn VARCHAR (100)) RETURNS INT BEGIN RETURN SELECT cid FROM clients c WHERE cname = cn; END;

SELECT client_id('Rabbit Foods Inc');
SELECT * from branches b where b.cid = client_id('Rabbit Foods Inc');

ROLLBACK;

