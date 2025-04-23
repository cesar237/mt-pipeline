#!/bin/bash

echo "Compiling pipeline app..."
gcc -lpthread pipeline-app.c argparse.c -o pipeline-app

echo "Compiling Run-to-completion app..."
gcc -lpthread rtc-app.c argparse.c -o rtc-app