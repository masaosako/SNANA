// =============================
//   sntools_genGauss_asym.h
// =============================

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <ctype.h>

#include <gsl/gsl_errno.h>
#include <gsl/gsl_spline.h>

#include "sntools.h"
#include "sntools_genGauss_asym.h"
#include "skewNormal.c"


// ******************************
void init_GENGAUSS_ASYM(GENGAUSS_ASYM_DEF *genGauss, double VAL ) {

  // Created Apr 13 2016 by R.Kessler
  // Init all genGasuss parameter values to VAL .

  sprintf(genGauss->NAME,"NULL");

  genGauss->FUNINDEX  = -9 ;  // Feb 18 2018
  genGauss->PEAK      = VAL ;

  genGauss->RANGE[0]  = VAL ;
  genGauss->RANGE[1]  = VAL ;

  genGauss->SIGMA[0]  = VAL ;
  genGauss->SIGMA[1]  = VAL ;

  // SKEW params = 0 regardless of VAL
  genGauss->SKEW[0]       = 0.0 ; 
  genGauss->SKEW[1]       = 0.0 ;  
  genGauss->SKEWNORMAL[0] = 0.0 ;
  genGauss->SKEWNORMAL[1] = 0.0 ;
  genGauss->SKEWNORMAL[2] = 0.0 ;

  genGauss->NGRID     = 0 ;
  genGauss->FUNINDEX  = 0;

  // Mar 29 2017: 2nd peak
  genGauss->PROB2     = 0.0 ;
  genGauss->PEAK2     = 0.0 ;
  genGauss->SIGMA2[0] = 0.0 ;
  genGauss->SIGMA2[1] = 0.0 ;


  // Jun 2018 RMS
  genGauss->RMS = 0.0 ;

} // end init_GENGAUSS_ASYM

// ************************************
void copy_GENGAUSS_ASYM(GENGAUSS_ASYM_DEF *genGauss1,
			GENGAUSS_ASYM_DEF *genGauss2) {

  // Aug 30 2016
  // copy contents of genGauss1 into genGauss2
  int i ;

  sprintf(genGauss2->NAME,"%s", genGauss1->NAME );

  genGauss2->PEAK      =  genGauss1->PEAK ;
  genGauss2->NGRID     =  genGauss1->NGRID ;

  for(i=0; i < 2; i++ ) {
    genGauss2->RANGE[i]  = genGauss1->RANGE[i] ;
    genGauss2->SIGMA[i]  = genGauss1->SIGMA[i] ;
    genGauss2->SKEW[i]   = genGauss1->SKEW[i] ;
  }

  for(i=0; i < 3; i++ ) 
    { genGauss2->SKEWNORMAL[i] = genGauss1->SKEWNORMAL[i] ; }
  
  genGauss2->FUNINDEX = genGauss1->FUNINDEX ;

  // 2nd peak
  genGauss2->PROB2      =  genGauss1->PROB2 ;
  genGauss2->PEAK2      =  genGauss1->PEAK2 ;
  genGauss2->SIGMA2[0]  =  genGauss1->SIGMA2[0] ;
  genGauss2->SIGMA2[1]  =  genGauss1->SIGMA2[1] ;


  return ;

} // end copy_GENGAUSS_ASYM

// **********************************
double exec_GENGAUSS_ASYM(GENGAUSS_ASYM_DEF *genGauss) {

  // Created May 2012: 
  // return random Guassian number with above params
  // If siglo & sighi > 100*(hi-lo), then just assume a flat
  // distribution between lo & hi.
  // Same as BIGAUSRAN, but here a struct is passed with all the args.
  //
  // Mar 16 2014: use new SKEW parameter
  // Apr 14 2016: sigmax -> 10*range [was 100*range]
  // Apr 20 2016: check NGRID option to snap to grid
  // Aug 30 2016: add call to skewNormal().
  // Oct 02 2016: compute rangeDif after DO_GRID if-block (bug fix)
  // Mar 29 2017: check for 2nd peak
  // Feb 28 2018: check for correlated randoms (see 'redCor')
  //
  double peak, lo, hi, siglo, sighi, skewlo, skewhi, xlo, xhi ; 
  double gridsize, grid0, skewNormal[3] ;
  int NTRY, DO_SKEWSIGMA, DO_SKEWNORMAL, DO_GRID;
  int NGRID, FUNINDEX, j ;
  int USE_PEAK1=1, MXTRY = 1000, LDMP=0 ;
  double ranval=0.0, rangeDif, RANGE[2], sigmax, ran1, ran2, PROB2 ;
  char fnam[] = "exec_GENGAUSS_ASYM" ;

  // ---------- BEGIN -------------

  // always burn random to stay synced.
  ran1 = FlatRan1(1) ;

  // check optional 2nd peak (Mar 2017)
  PROB2 = genGauss->PROB2 ;
  if ( PROB2 > 0.0000001 ) {
    ran2 = FlatRan1(1) ;
    if ( ran2 < PROB2 ) {
      peak        = genGauss->PEAK2 ;
      siglo       = genGauss->SIGMA2[0] ;
      sighi       = genGauss->SIGMA2[1] ;
      skewlo = skewhi = 0.0 ;
      for(j=0;j<3;j++) { skewNormal[j] = 0.0 ; }
      USE_PEAK1   = 0;
    }
  }

  if ( USE_PEAK1 ) {
    peak        = genGauss->PEAK ;
    siglo       = genGauss->SIGMA[0] ;
    sighi       = genGauss->SIGMA[1] ;
    skewlo      = genGauss->SKEW[0] ;
    skewhi      = genGauss->SKEW[1] ;
    for(j=0;j<3;j++) { skewNormal[j]  = genGauss->SKEWNORMAL[j]; }
  }

  lo          = genGauss->RANGE[0] ;
  hi          = genGauss->RANGE[1] ;
  NGRID       = genGauss->NGRID ;
  FUNINDEX    = genGauss->FUNINDEX ;

  RANGE[0]=lo; RANGE[1]=hi;

  // abort on crazy NGRID value
  if ( NGRID < 0 || NGRID > 100 ) {
    print_preAbort_banner(fnam);
    printf("\t peak = %f \n", peak);
    printf("\t range(lo,hi) = %f, %f \n", lo, hi );
    printf("\t sigma(lo,hi) = %f, %f \n", siglo, sighi );
    sprintf(c1err,"Crazy NGRID=%d", NGRID );
    errmsg(SEV_FATAL, 0, fnam, c1err, "" );
  }


  gridsize = grid0 = -9.0 ;
  DO_GRID = ( NGRID >=2 && hi>lo && siglo>0.0 && sighi>0.0 );
  if ( DO_GRID ) {
    // compute gridsize here to allow command-line override of RANGE
    gridsize = (hi-lo)/(double)(NGRID-1) ;
    grid0    = lo ; // store first grid location
    lo   -= gridsize/2.0 ;
    hi   += gridsize/2.0 ;
  } 

  rangeDif = hi-lo;
  NTRY  = 0;
  sigmax = 10.*rangeDif ;
  DO_SKEWSIGMA  = ( fabs(skewlo)  > 1.0E-9 || fabs(skewhi) > 1.0E-9 ) ;
  DO_SKEWNORMAL = ( skewNormal[1] > 1.0E-9 ) ;


  if ( DO_SKEWSIGMA && DO_SKEWNORMAL ) { 
    sprintf(c1err,"Cannot generate SKEWSIG and SKEWNORMAL");
    sprintf(c2err,"SKEW=%.3f,%.3f  SKEWNORMAL=%.3f,%.3f,%.3f",
	    skewlo,skewhi, skewNormal[0],skewNormal[2],skewNormal[2] );
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err );
  }


  if ( LDMP ) {
    printf("\t xxx ----------------- %s ------------ \n", genGauss->NAME);
    printf("\t xxx peak = %f \n", peak);
    printf("\t xxx range(lo,hi) = %f, %f \n", lo, hi );
    printf("\t xxx sigma(lo,hi) = %f, %f \n", siglo, sighi );
    printf("\t xxx DO_GRID=%d  sigmax=%f \n", DO_GRID, sigmax);
    printf("\t xxx rangeDif=%f   ran1=%f \n", rangeDif, ran1);
    fflush(stdout);
  }

  // always burn random to stay synced.

  if ( lo == hi ) {
    ranval = lo ;    // delta function
  }
  else if ( siglo > sigmax && sighi > sigmax ) {
    ranval = lo + rangeDif*ran1 ;  // flat distribution
  }
  else {
  GENVAL:
    NTRY++ ;
    if ( NTRY > MXTRY ) {
      print_preAbort_banner(fnam);
      dump_GENGAUSS_ASYM(genGauss);
      printf(" DO_SKEW[SIGMA,NORMAL] = %d, %d \n", 
	     DO_SKEWSIGMA, DO_SKEWNORMAL);

      sprintf(c1err,"Could not find %s RANDOM after %d tries ", 
	      genGauss->NAME, NTRY );
      sprintf(c2err,"Something is crazy.");
      errmsg(SEV_FATAL, 0, fnam, c1err, c2err );
    }
    
    if ( DO_SKEWSIGMA  )  {
      xlo = lo - peak ;
      xhi = hi - peak ;
      ranval = peak + skewGaussRan(xlo,xhi,siglo,sighi,skewlo,skewhi); 
    }
    else if ( DO_SKEWNORMAL ) {

      sprintf(c1err,"SKEWNORMAL no longer supported.");
      sprintf(c2err,"Need to pass SEED");
      errmsg(SEV_FATAL, 0, fnam, c1err, c2err );

      /* xxxx  Jun 8, 2018
	 removed include snlc_sim.h, hence no INPUTS.SEED

      // Pass seed and 3 parameters defining skewNormal
      ranval  = skewNormalRan(INPUTS.ISEED, 
			      skewNormal[0], skewNormal[1], skewNormal[2] );
      */
    }
    else { 
      ranval = peak + biGaussRan(siglo,sighi) ; 

    }
    
    if ( ranval < lo ) { goto GENVAL;  } 
    if ( ranval > hi ) { goto GENVAL;  } 
  }


  // April 2016: check option to snap to grid
  if ( DO_GRID ) {
    double ranval_orig = ranval ;
    int    ibin = (int)((ranval_orig-lo)/gridsize) ;
    double xbin = (double)ibin ;
    ranval      = grid0 + (xbin*gridsize) ;

    /*
    printf(" xxx ranval=%.3f->0.3f  ibin=%d  grid0=%.3f\n",
    ranval_orig, ranval, ibin, grid0); fflush(stdout); */
  }

  if ( LDMP ) {
    printf("\t xxx ranval = %f \n", ranval); 
    fflush(stdout);
  }

  return(ranval) ;

} // end of exec_GENGAUSS_ASYM


// ****************************************
void dump_GENGAUSS_ASYM(GENGAUSS_ASYM_DEF *genGauss) {

  // Created Sep 2 2016 
  // screen-Dump contents of genGauss

  double *ptrVal ;
  char fnam[] = "dump_GENGAUSS_ASYM" ;

  // ---------- BEGIN -----------

  printf("\n");
  printf("# ------------------------------------------- \n");
  printf(" START %s  for '%s' (INDEX=%d) \n",
	 fnam, genGauss->NAME, genGauss->FUNINDEX);

  printf("\t PEAK = %.3f  \n", genGauss->PEAK);

  ptrVal = genGauss->RANGE ;
  printf("\t RANGE = %.3f to %.3f \n", 	ptrVal[0], ptrVal[1] );

  ptrVal = genGauss->SIGMA; 
  printf("\t SIGMA(-/+) = %.3f / %.3f \n", ptrVal[0], ptrVal[1] );
	 
  ptrVal = genGauss->SKEWNORMAL ; 
  printf("\t SKEWNORMAL(LOC,SCALE,SKEW) = %.3f , %.3f , %.3f\n", 
	 ptrVal[0], ptrVal[1], ptrVal[2] );
 
  printf(" END %s \n", fnam );
  printf("# ------------------------------------------- \n");

  fflush(stdout);

  return ;

} // end dump_GENGAUSS_ASYM


