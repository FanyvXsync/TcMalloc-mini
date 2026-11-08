UnitTest:UnitTest.cc
	g++ -o $@ $^ -std=c++17
.PHONY:clean
clean:
	rm -f UnitTest