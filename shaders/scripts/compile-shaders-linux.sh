#! /bin/bash

trash spv/comp.spv;
glslang -V gradient.comp;
mv comp.spv spv;
