CC=g++ -g -Wall -fno-builtin -std=c++17

# List of source files for your pager
PAGER_SOURCES=vm_pager.cpp

# Generate the names of the pager's object files
PAGER_OBJS=${PAGER_SOURCES:.cpp=.o}

all: pager app0 app1 app2 app3 app4 app5 app6 app7 app8 app9 app10 app11 app12 app13 app14 app15 app16 app17 app18 app19 app20 app21 app22 app23 app24 app25 app26 app27 app28 app29

# Compile the pager and tag this compilation
pager: ${PAGER_OBJS} libvm_pager.o
	./autotag.sh
	${CC} -o $@ $^

# Compile an application program
app0: test0.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app1: test1.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app2: test2.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app3: test3.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app4: test4.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app5: test5.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app6: test6.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app7: test7.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app8: test8.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app9: test9.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app10: test10.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app11: test11.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app12: test12.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app13: test13.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app14: test14.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app15: test15.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app16: test16.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app17: test17.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app18: test18.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app19: test19.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app20: test20.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app21: test21.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app22: test22.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app23: test23.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app24: test24.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app25: test25.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app26: test26.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app27: test27.10.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app28: testclock.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

app29: testevict.4.cpp libvm_app.o
	${CC} -o $@ $^ -ldl

# Generic rules for compiling a source file to an object file
%.o: %.cpp
	${CC} -c $<
%.o: %.cc
	${CC} -c $<

clean:
	rm -f ${PAGER_OBJS} pager app0 app1 app2 app3 app4 app5 app6 app7 app8 app9 app10 app11 app12 app13 app14 app15 app16 app17 app18 app19 app20 app21 app22 app23 app24 app25 app26 app27 app28 app29
