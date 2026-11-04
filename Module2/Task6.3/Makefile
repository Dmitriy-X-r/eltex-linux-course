CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g
PICFLAGS = -fPIC

TARGET = calculator
PLUGIN_DIR = plugins

PLUGINS = $(PLUGIN_DIR)/libadd.so \
          $(PLUGIN_DIR)/libsubtract.so \
          $(PLUGIN_DIR)/libmultiply.so \
          $(PLUGIN_DIR)/libdivide.so

all: $(TARGET) $(PLUGINS)

$(TARGET): main.o
	$(CC) $(CFLAGS) -o $(TARGET) main.o -ldl

main.o: main.c calculator.h
	$(CC) $(CFLAGS) -c main.c

$(PLUGIN_DIR)/add.o: $(PLUGIN_DIR)/add.c calculator.h
	$(CC) $(CFLAGS) $(PICFLAGS) -I. -c $< -o $@

$(PLUGIN_DIR)/subtract.o: $(PLUGIN_DIR)/subtract.c calculator.h
	$(CC) $(CFLAGS) $(PICFLAGS) -I. -c $< -o $@

$(PLUGIN_DIR)/multiply.o: $(PLUGIN_DIR)/multiply.c calculator.h
	$(CC) $(CFLAGS) $(PICFLAGS) -I. -c $< -o $@

$(PLUGIN_DIR)/divide.o: $(PLUGIN_DIR)/divide.c calculator.h
	$(CC) $(CFLAGS) $(PICFLAGS) -I. -c $< -o $@

$(PLUGIN_DIR)/libadd.so: $(PLUGIN_DIR)/add.o
	$(CC) -shared -o $@ $<

$(PLUGIN_DIR)/libsubtract.so: $(PLUGIN_DIR)/subtract.o
	$(CC) -shared -o $@ $<

$(PLUGIN_DIR)/libmultiply.so: $(PLUGIN_DIR)/multiply.o
	$(CC) -shared -o $@ $<

$(PLUGIN_DIR)/libdivide.so: $(PLUGIN_DIR)/divide.o
	$(CC) -shared -o $@ $<

clean:
	rm -f $(TARGET) main.o $(PLUGIN_DIR)/*.o $(PLUGINS)

.PHONY: all clean
