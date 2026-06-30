#pragma once
#include "query_engine.h"
#include "disk_engine.h"
#include "column_schema.h"
#include <string>
#include <vector>
#include <ostream>
#include <iostream>

class UIPanelB {
public:
    // avl_metrics: resultado de avl.metricasString() ya como string.
    static void display(const QueryResult&           result,
                        DiskEngine&                  engine,
                        const std::string&           avl_metrics,
                        const std::vector<ColumnDef>& schema,
                        std::ostream&                out = std::cout);
};
