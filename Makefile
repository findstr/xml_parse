.PHONY: clean

xml: array.o xml.o xml_str.o xml_test.o
	gcc -o $@ $^

clean:
	del *.o
	del *.exe

array.o: array.c array.h
	gcc -c $<
xml.o: xml.cpp xml.h
	gcc -c $<
xml_str.o: xml_str.cpp xml_str.h
	gcc -c $<
xml_test.o: xml_test.cpp
	gcc -c $<

