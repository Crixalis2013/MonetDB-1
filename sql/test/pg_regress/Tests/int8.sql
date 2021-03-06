set optimizer = 'sequential_pipe'; -- to get predictable errors

--
-- bigint
-- Test bigint 64-bit integers.
--
CREATE TABLE INT8_TBL(q1 bigint, q2 bigint);

INSERT INTO INT8_TBL VALUES('  123   ','  456');
INSERT INTO INT8_TBL VALUES('123   ','4567890123456789');
INSERT INTO INT8_TBL VALUES('4567890123456789','123');
INSERT INTO INT8_TBL VALUES('4567890123456789','4567890123456789');
INSERT INTO INT8_TBL VALUES('4567890123456789','-4567890123456789');

-- bad inputs
INSERT INTO INT8_TBL(q1) VALUES ('      ');
INSERT INTO INT8_TBL(q1) VALUES ('xxx');
INSERT INTO INT8_TBL(q1) VALUES ('3908203590239580293850293850329485');
INSERT INTO INT8_TBL(q1) VALUES ('-1204982019841029840928340329840934');
INSERT INTO INT8_TBL(q1) VALUES ('- 123');
INSERT INTO INT8_TBL(q1) VALUES ('  345     5');
INSERT INTO INT8_TBL(q1) VALUES ('');

SELECT * FROM INT8_TBL;

SELECT '' AS five, q1 AS plus, -q1 AS minus FROM INT8_TBL;

SELECT '' AS five, q1, q2, q1 + q2 AS plus FROM INT8_TBL;
SELECT '' AS five, q1, q2, q1 - q2 AS minus FROM INT8_TBL;
SELECT '' AS three, q1, q2, q1 * q2 AS multiply FROM INT8_TBL WHERE q2 <> 4567890123456789 ORDER BY q1, q2;
SELECT '' AS three, q1, q2, q1 * q2 AS multiply FROM INT8_TBL
 WHERE q1 < 1000 or (q2 > 0 and q2 < 1000);
SELECT '' AS five, q1, q2, q1 / q2 AS divide FROM INT8_TBL;

SELECT '' AS five, q1, cast(q1 as double) FROM INT8_TBL;
SELECT '' AS five, q2, cast(q2 as double) FROM INT8_TBL;

SELECT '' AS five, 2 * q1 AS "twice integer" FROM INT8_TBL;
SELECT '' AS five, q1 * 2 AS "twice integer" FROM INT8_TBL;

-- TO_CHAR()
-- Note: this PostgreSQL (and Oracle) function is NOT supported by MonetDB, so all to_char(<expr>, '<format>') queries will fail with Error: SELECT: no such binary operator 'to_char(bigint,char)'
SELECT '' AS to_char_1, to_char(q1, '9G999G999G999G999G999'), to_char(q2, '9,999,999,999,999,999') 
	FROM INT8_TBL;

SELECT '' AS to_char_2, to_char(q1, '9G999G999G999G999G999D999G999'), to_char(q2, '9,999,999,999,999,999.999,999') 
	FROM INT8_TBL;	

SELECT '' AS to_char_3, to_char( (q1 * -1), '9999999999999999PR'), to_char( (q2 * -1), '9999999999999999.999PR') 
	FROM INT8_TBL;

SELECT '' AS to_char_4, to_char( (q1 * -1), '9999999999999999S'), to_char( (q2 * -1), 'S9999999999999999') 
	FROM INT8_TBL;

SELECT '' AS to_char_5,  to_char(q2, 'MI9999999999999999')     FROM INT8_TBL;	
SELECT '' AS to_char_6,  to_char(q2, 'FMS9999999999999999')    FROM INT8_TBL;
SELECT '' AS to_char_7,  to_char(q2, 'FM9999999999999999THPR') FROM INT8_TBL;
SELECT '' AS to_char_8,  to_char(q2, 'SG9999999999999999th')   FROM INT8_TBL;	
SELECT '' AS to_char_9,  to_char(q2, '0999999999999999')       FROM INT8_TBL;	
SELECT '' AS to_char_10, to_char(q2, 'S0999999999999999')      FROM INT8_TBL;	
SELECT '' AS to_char_11, to_char(q2, 'FM0999999999999999')     FROM INT8_TBL;	
SELECT '' AS to_char_12, to_char(q2, 'FM9999999999999999.000') FROM INT8_TBL;
SELECT '' AS to_char_13, to_char(q2, 'L9999999999999999.000')  FROM INT8_TBL;	
SELECT '' AS to_char_14, to_char(q2, 'FM9999999999999999.999') FROM INT8_TBL;
SELECT '' AS to_char_15, to_char(q2, 'S 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 . 9 9 9') FROM INT8_TBL;
SELECT '' AS to_char_16, to_char(q2, E'99999 "text" 9999 "9999" 999 "\\"text between quote marks\\"" 9999') FROM INT8_TBL;
SELECT '' AS to_char_17, to_char(q2, '999999SG9999999999')     FROM INT8_TBL;

-- cleanup created table
DROP TABLE INT8_TBL;

set optimizer = 'default_pipe';
