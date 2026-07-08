#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_log () {
  echo -e "\n${BLUE}Printing log: $1${NC}"
  cat "$1"
}

check_result () {
  result=$(grep -c "Test passed." "$1")
  if [[ $result -eq 1 ]]
  then
    echo -e "\n${GREEN}Test passed."
  else
    echo -e "\n${RED}Test failed."
    exit 1
  fi
}

echo -e "${BLUE}Test: handle illegal tftp operation.${NC}"
cd build || exit
echo "Starting server."
./tftp-server | tee server.log 2>&1 &
sleep 2
cd ../css432-tftp/build || exit
echo "Starting test client."
./tftp-client e "dummy.txt" 4 > client.log 2>&1
kill "$(jobs -p)"
mv client.log ../../build/
cd ../../build || exit
print_log server.log
print_log client.log
check_result client.log
rm client.log server.log
cd .. || exit

echo -e "${BLUE}Test: handle file not exist.${NC}"
cd build || exit
echo "Starting server."
./tftp-server > server.log 2>&1 &
sleep 2
cd ../css432-tftp/build || exit
echo "Starting test client."
./tftp-client r "notexist.txt" 1 > client.log 2>&1
kill "$(jobs -p)"
mv client.log ../../build/
cd ../../build || exit
print_log server.log
print_log client.log
check_result client.log
rm client.log server.log
cd .. || exit

echo -e "${BLUE}Test: handle file already exist.${NC}"
cd build || exit
echo "Starting server."
./tftp-server > server.log 2>&1 &
sleep 2
cd ../css432-tftp/build || exit
echo "Starting test client."
./tftp-client w "already-in-server.txt" 6 > client.log 2>&1
kill "$(jobs -p)"
mv client.log ../../build/
cd ../../build || exit
print_log server.log
print_log client.log
check_result client.log
rm client.log server.log
cd .. || exit
