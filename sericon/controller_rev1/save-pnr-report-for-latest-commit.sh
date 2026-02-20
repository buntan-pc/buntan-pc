#!/bin/sh

COMMIT_ID=$(git rev-parse HEAD)
REPORT_PATH="impl/pnr/controller.rpt.txt"

cp "$REPORT_PATH" "$REPORT_PATH.$COMMIT_ID"

echo "saved to $REPORT_PATH.$COMMIT_ID"
