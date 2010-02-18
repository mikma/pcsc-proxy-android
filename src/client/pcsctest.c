/***************************************************************************
    begin       : Wed Jan 20 2010
    copyright   : (C) 2010 by Martin Preuss
    email       : martin@libchipcard.de

 ***************************************************************************
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Lesser General Public            *
 *   License as published by the Free Software Foundation; either          *
 *   version 2.1 of the License, or (at your option) any later version.    *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston,                 *
 *   MA  02111-1307  USA                                                   *
 *                                                                         *
 ***************************************************************************/


#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <pcsclite.h>
#include <winscard.h>

#include <stdio.h>
#include <stdlib.h>


#ifdef ENABLE_DEBUG
# define DEBUGP(format, args...) \
  fprintf(stderr, __FILE__":%5d:" format, __LINE__, ## args);
#else
# define DEBUGP(format, args...)
#endif



int test1(int argc, char **argv) {
  SCARDCONTEXT hContext;
  LPSTR mszGroups;
  DWORD dwGroups;
  LONG rv;
  const char *p;

  rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
  if (rv!=SCARD_S_SUCCESS) {
    DEBUGP("ERROR: here (%08x)\n", (int) rv);
    return 2;
  }
  rv = SCardListReaderGroups(hContext, NULL, &dwGroups);
  if (rv!=SCARD_S_SUCCESS) {
    DEBUGP("ERROR: here (%08x)\n", (int) rv);
    return 2;
  }
  mszGroups = malloc(sizeof(char)*dwGroups);
  rv = SCardListReaderGroups(hContext, mszGroups, &dwGroups);
  if (rv!=SCARD_S_SUCCESS) {
    DEBUGP("ERROR: here (%08x)\n", (int) rv);
    return 2;
  }

  p=mszGroups;
  fprintf(stdout, "Reader Groups:\n");
  while(*p) {
    fprintf(stdout, " %s\n", p);
    while(*(p++));
  }

  fprintf(stderr, "Ok.\n");
  return 0;
}



int test2(int argc, char **argv) {
  SCARDCONTEXT hContext;
  LPSTR mszReaders;
  DWORD dwReaders;
  LONG rv;
  const char *p;

  rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
  if (rv!=SCARD_S_SUCCESS) {
    DEBUGP("ERROR: here (%08x)\n", (int) rv);
    return 2;
  }
  rv = SCardListReaders(hContext, NULL, NULL, &dwReaders);
  if (rv!=SCARD_S_SUCCESS && rv!=SCARD_E_INSUFFICIENT_BUFFER) {
    DEBUGP("ERROR: here (%08x)\n", (int) rv);
    return 2;
  }
  mszReaders = malloc(sizeof(char)*dwReaders);
  rv = SCardListReaders(hContext, NULL, mszReaders, &dwReaders);
  if (rv!=SCARD_S_SUCCESS) {
    DEBUGP("ERROR: here (%08x)\n", (int) rv);
    return 2;
  }

  p=mszReaders;
  fprintf(stdout, "Readers:\n");
  while(*p) {
    fprintf(stdout, " %s\n", p);
    while(*(p++));
  }

  return 0;
}


int main(int argc, char **argv) {
  return test2(argc, argv);
}


