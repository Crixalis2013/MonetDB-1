CREATE TABLE experiment( 
	run date DIMENSION[  TIMESTAMP '2010-01-01':*: INTERVAL'1' day], 
	payload float ARRAY[4][4] DEFAULT 0.0 );
