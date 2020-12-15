CREATE TABLE voyages (
	number mediumint,
	number_sup character(1),
	trip mediumint,
	trip_sup character(1),
	boatname varchar(50),
	master varchar(50),
	tonnage mediumint,
	type_of_boat varchar(30),
	built varchar(15),
	bought varchar(15),
	hired varchar(15),
	yard character(1),
	chamber character(1),
	departure_date varchar(15),
	departure_harbour varchar(30),
	cape_arrival varchar(15),
	cape_departure varchar(15),
	arrival_date varchar(15),
	arrival_harbour varchar(30),
	next_voyage mediumint,
	particulars varchar(507)
);

select c.name, c.type, c.type_digits, c.type_scale, c."null", c."default", c.number from sys._columns c, sys._tables t, sys.schemas s
where c.table_id = t.id and t.name = 'voyages' and t.schema_id = s.id and s.name = 'sys' order by c.number;

drop table voyages;
