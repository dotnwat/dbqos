#!/bin/sh

CPLEX_JAR=$CPLEX_HOME/cplex/lib/cplex.jar
CPLEX_LIB=$CPLEX_HOME/cplex/bin/x86-64_sles10_4.1/

CLASS=$1
shift
ARGS=$*

java -cp $CPLEX_JAR:. -Djava.library.path=$CPLEX_LIB $CLASS $ARGS
