#include <string.h>
#include <iostream>
#include <stdio.h>

#include "gmtp.h"

#define OFF "off"
#define ON "on"

using namespace std;

int main (int argc, char *argv[])
{
  string s(argv[1]);

  if(s.compare(ON) == 0) {
	  enable_gmtp_inter();
  }

  if(s.compare(OFF) == 0) {
	  disable_gmtp_inter();
  }

  return 0;
}
