#!/bin/bash

cd os161-1.99
cd kern/compile/ASST1
bmake depend
bmake
bmake install
