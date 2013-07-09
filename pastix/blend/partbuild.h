/************************************************************/
/**                                                        **/
/**   NAME       : partbuild.h                             **/
/**                                                        **/
/**   AUTHORS    : Pascal HENON                            **/
/**                                                        **/
/**   FUNCTION   : Part of a parallel direct block solver. **/
/**                make a new symbol matrix  from          **/
/**                a symbol matrix and extrasymbol matrix  **/
/**                                                        **/
/**   DATES      : # Version 0.0  : from : 22 jul 1998     **/
/**                                 to     09 sep 1998     **/
/**                                                        **/
/************************************************************/
#ifndef PARTBUILD_H
#define static
#endif

void         partBuild(SymbolMatrix *, ExtraSymbolMatrix *, CostMatrix *, ExtraCostMatrix *, BlendCtrl *, const Dof *);

#undef static
