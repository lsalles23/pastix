/**
 *
 * @file scotch_strats.h
 *
 *  PaStiX order routines
 *  PaStiX is a software package provided by Inria Bordeaux - Sud-Ouest,
 *  LaBRI, University of Bordeaux 1 and IPB.
 *
 * Contains (PT-)Scotch strategy strings
 *
 * @version 5.1.0
 * @author Francois Pellegrini
 * @author Pierre Ramet
 * @author Xavier Lacoste
 * @author Mathieu Faverge
 * @date 2013-06-24
 *
 **/
#ifndef _SCOTCH_STRATS_H_
#define _SCOTCH_STRATS_H_

#define SCOTCH_STRAT_DIRECT                                             \
  "c{rat=0.7,"                                                          \
  """cpr=n{sep=/(vert>120)?m{rat=0.8,"                                  \
  ""                        "vert=100,"                                 \
  ""                        "low=h{pass=10},"                           \
  ""                        "asc=f{bal=0.2}}|"                          \
  ""                      "m{rat=0.8,"                                  \
  ""                        "vert=100,"                                 \
  ""                        "low=h{pass=10},"                           \
  ""                        "asc=f{bal=0.2}};,"                         \
  ""      "ole=f{cmin=0,cmax=100000,frat=0.0},"                       \
  ""      "ose=g},"                                                     \
  """unc=n{sep=/(vert>120)?(m{rat=0.8,"                                 \
  ""                         "vert=100,"                                \
  ""                         "low=h{pass=10},"                          \
  ""                         "asc=f{bal=0.2}})|"                        \
  ""                        "m{rat=0.8,"                                \
  ""                          "vert=100,"                               \
  ""                          "low=h{pass=10},"                         \
  ""                          "asc=f{bal=0.2}};,"                       \
  ""      "ole=f{cmin=15,cmax=100000,frat=0.08},"                       \
  ""      "ose=g}}"

#define SCOTCH_STRAT_INCOMP                                             \
  "c{rat=0.7,"                                                          \
  """cpr=n{sep=/(vert>120)?m{vert=100,"                                 \
  ""                        "low=h{pass=10},"                           \
  ""                        "asc=f{bal=0.2}}|"                          \
  ""                      "m{vert=100,"                                 \
  ""                        "low=h{pass=10},"                           \
  ""                        "asc=f{bal=0.2}};,"                         \
  ""      "ole=f{cmin=0,cmax=100000,frat=0.08},"                        \
  ""      "ose=g},"                                                     \
  """unc=n{sep=/(vert>120)?(m{vert=100,"                                \
  ""                         "low=h{pass=10},"                          \
  ""                         "asc=f{bal=0.2}})|"                        \
  ""                       "m{vert=100,"                                \
  ""                         "low=h{pass=10},"                          \
  ""                         "asc=f{bal=0.2}};,"                        \
  ""      "ole=f{cmin=15,cmax=100000,frat=0.08},"                       \
  ""            "ose=g}}"
#define SCOTCH_STRAT_PERSO                                              \
  "c{rat=0.7,"                                                          \
  """cpr=n{sep=/(vert>%ld)?m{vert=100,"                                 \
  ""                        "low=h{pass=10},"                           \
  ""                        "asc=f{bal=0.2}}|"                          \
  ""                      "m{vert=100,"                                 \
  ""                        "low=h{pass=10},"                           \
  ""                        "asc=f{bal=0.2}};,"                         \
  ""      "ole=f{cmin=%ld,cmax=%ld,frat=%f},"                           \
  ""      "ose=g},"                                                     \
  """unc=n{sep=/(vert>%ld)?(m{vert=100,"                                \
  ""                         "low=h{pass=10},"                          \
  ""                         "asc=f{bal=0.2}})|"                        \
  ""                       "m{vert=100,"                                \
  ""                         "low=h{pass=10},"                          \
  ""                         "asc=f{bal=0.2}};,"                        \
  ""      "ole=f{cmin=%ld,cmax=%ld,frat=%f},"                           \
  ""      "ose=g}}"

#define PTSCOTCH_STRAT_DIRECT                                           \
  "c{rat=0.7,"                                                          \
  """cpr=n{sep=/(vert>120)?m{rat=0.8,"                                 \
  ""                        "vert=100,"                                \
  ""                        "low=h{pass=10},"                          \
  ""                        "asc=f{bal=0.2}}|"                         \
  ""                      "m{rat=0.8,"                                 \
  ""                        "vert=100,"                                \
  ""                        "low=h{pass=10},"                          \
  ""                        "asc=f{bal=0.2}};,"                        \
  ""      "ole=f{cmin=0,cmax=100000,frat=0.0},"                         \
  ""      "ose=g},"                                                     \
  """unc=n{sep=/(vert>120)?(m{type=h,"                                  \
  ""                         "rat=0.8,"                                 \
  ""                         "vert=100000,"                             \
  ""                         "low=h{pass=10},"                          \
  ""                         "asc=f{bal=08.2}})|"                       \
  ""                       "m{type=h,"                                  \
  ""                         "rat=0.8,"                                 \
  ""                         "vert=100,"                                \
  ""                         "low=h{pass=10},"                          \
  ""                         "asc=f{bal=0.2}};,"                        \
  ""      "ole=f{cmin=15,cmax=100000,frat=0.08},"                       \
  ""      "ose=g}}"
#define PTSCOTCH_STRAT_INCOMP                                           \
  "c{rat=0.7,"                                                          \
  """cpr=n{sep=/(vert>120)?m{vert=100,"                                 \
  ""                        "low=h{pass=10},"                           \
  ""                        "asc=f{bal=0.2}}|"                          \
  ""                      "m{vert=100,"                                 \
  ""                        "low=h{pass=10},"                           \
  ""                        "asc=f{bal=0.2}};,"                         \
  ""      "ole=f{cmin=0,cmax=100000,frat=0.08},"                        \
  ""      "ose=g},"                                                     \
  """unc=n{sep=/(vert>120)?(m{vert=100,"                                \
  ""                         "low=h{pass=10},"                          \
  ""                         "asc=f{bal=0.2}})|"                        \
  ""                       "m{vert=100,"                                \
  ""                         "low=h{pass=10},"                          \
  ""                         "asc=f{bal=0.2}};,"                        \
  ""      "ole=f{cmin=15,cmax=100000,frat=0.08},"                       \
  ""      "ose=g}}"

#define PTSCOTCH_STRAT_PERSO  "c{rat=0.7,cpr=n{sep=/(vert>%ld)?m{vert=100,low=h{pass=10},asc=f{bal=0.2}}|m{vert=100,low=h{pass=10},asc=f{bal=0.2}};,ole=f{cmin=%ld,cmax=%ld,frat=%f},ose=g},unc=n{sep=/(vert>%ld)?(m{vert=100,low=h{pass=10},asc=f{bal=0.2}})|m{vert=100,low=h{pass=10},asc=f{bal=0.2}};,ole=f{cmin=%ld,cmax=%ld,frat=%f},ose=g}}"


#endif /* _SCOTCH_STRATS_H_ */
