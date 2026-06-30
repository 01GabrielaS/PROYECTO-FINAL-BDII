CXX = c++
CXXFLAGS = -std=c++17 -I.

SRCS = \
    apps/main_demo.cpp \
    apps/ui_panel_b.cpp \
    engine/disk_engine.cpp \
    hardware/disk_geometry.cpp \
    hardware/disk_writer.cpp \
    hardware/free_bitmap.cpp \
    hardware/sector_header.cpp \
    parsing/schema_parser.cpp \
    parsing/validator.cpp \
    storage/column_schema.cpp \
    storage/csv_loader.cpp \
    storage/record_serializer.cpp \
    storage/record_id.cpp \
    indexing/index_manager.cpp

TARGET = apps/main_demo

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET) apps/main_demo_test

.PHONY: all clean
