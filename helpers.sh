#!/bin/bash

function auto_update() {
    inotifywait -m -e modify,create,delete . | while read path _ file; do
    # Execute the command here when a change occurs
    echo "File changed: $file"
    rsync -zvr . root@paradoxe-8:
    # Replace the echo with your actual command
    done
}
