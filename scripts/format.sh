#!/bin/bash

# Run clang-format on all source code
find kernel -name '*.[ch]' -exec clang-format -i --verbose {} + ;