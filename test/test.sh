#!/bin/sh
EXPECT="1
1
1
1
1
1
0
1
0"
RESULT=`mysql --default-character-set utf8 test -B -N < test.sql`
if [ "$EXPECT" = "$RESULT" ];
then
	echo 'ok'
else
	echo 'fail'
fi
