/* Copyright (C) 2005-2011 M. T. Homer Reid
 *
 * This file is part of SCUFF-EM.
 *
 * SCUFF-EM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * SCUFF-EM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * scuff-test-AssembleMatrix.cc -- a test program for libscuff's routines for
 *                              -- assembling the BEM matrix
 * 
 * homer reid                   -- 11/2005 -- 11/2011
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <libscuff.h>
#include <libscuffInternals.h>

using namespace scuff;

/***************************************************************/
/***************************************************************/
/***************************************************************/
int main(int argc, char *argv[]) 
{ 
  /***************************************************************/
  /* process command-line arguments ******************************/
  /***************************************************************/
  char *GeoFileName=0;
  cdouble Omega=1.0;
  char *ExportFileBase=0;
  char *ReadCache=0;
  char *WriteCache=0;
  int nThread=0;
  OptStruct OSArray[]=
   { {"geometry",       PA_STRING,  1, 1, (void *)&GeoFileName,    0, ".rwggeo file"},
     {"Omega",          PA_CDOUBLE, 1, 1, (void *)&Omega,          0, "file name of write cache"},
     {"ExportFileBase", PA_STRING,  1, 1, (void *)&ExportFileBase, 0, "base name of .hdf5/.m files for matrix export"},
     {"ReadCache",      PA_STRING,  1, 1, (void *)&ReadCache,      0, "file name of read cache"},
     {"WriteCache",     PA_STRING,  1, 1, (void *)&WriteCache,     0, "file name of write cache"},
     {"nThread",        PA_INT,     1, 1, (void *)&nThread,        0, "number of threads"},
     {0,0,0,0,0}
   };
  ProcessOptions(argc, argv, OSArray);
  if (GeoFileName==0)
   OSUsage(argv[0],OSArray,"--geometry option is mandatory");
  if (nThread==0)
   nThread=GetNumProcs();

  SetLogFileName("scuff-test-AssembleMatrix.log");

  /***************************************************************/
  /* create the geometry and allocate space for the BEM matrix   */
  /***************************************************************/
  RWGGeometry *G = new RWGGeometry(GeoFileName);
  G->SetLogLevel(SCUFF_VERBOSELOGGING);
  HMatrix *M=G->AllocateBEMMatrix();

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  if (ReadCache)
   { printf("Preloading FIPPI cache from file %s...\n",ReadCache);
     PreloadGlobalFIPPICache(ReadCache); 
     printf("...done!\n");
   };

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  Tic();
  printf("Starting BEM matrix assembly...\n");
  G->AssembleBEMMatrix(Omega, M, nThread);
  printf("...done in %.3f seconds\n",Toc());

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  if (ExportFileBase)
   { void *pCC=HMatrix::OpenC2MLContext(ExportFileBase);
     M->ExportToMATLAB(pCC,"M_%s",z2s(Omega));
   };

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  if (WriteCache)
   { printf("Storing FIPPI cache to file %s...\n",WriteCache);
     StoreGlobalFIPPICache(WriteCache); 
     printf("...done!\n");
   };

}
