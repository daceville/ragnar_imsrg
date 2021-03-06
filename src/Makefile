CC = g++

# Compiler flags:
#  -DNO_ODE=1 compiles without boost/ode package used for flow equation solver
#  -DOPENBLAS_NOUSEOMP=1 removes parallel blocks which take threads away from OPENBLAS
#                        to be used if OpenBlas was compiled without the USE_OMP flat

INCLUDE   = -I./armadillo
FLAGS     = -O3 -march=native -std=c++11 -fopenmp -fPIC -ffast-math  #-flto
SOFLAGS   = -O3 -march=native -std=c++11 -fopenmp -fPIC -ffast-math  #-flto
WARNFLAGS = -Wall -Wno-comment -Wno-deprecated-declarations
FLAGS    += $(WARNFLAGS)
SOFLAGS  += $(WARNFLAGS)
INSTDIR = $(HOME)

ifeq ($(DEBUG),on)
 FLAGS     = -march=native -std=c++11 -fopenmp -fPIC -ffast-math  
 SOFLAGS   = -march=native -std=c++11 -fopenmp -fPIC -ffast-math  
 FLAGS += -g
 SOFLAGS += -g
endif

PYTHONFLAGS = -I/usr/include/python2.7 -lboost_python #-lpython 

THEHOST = $(shell if [ `hostname|grep jrl` ]; then echo jureca; elif [ `hostname|grep cougar` ]; then echo cougar; else echo other; fi)

ALL = libIMSRG.so
LIBS = -lopenblas -lgsl -lz -lhdf5_cpp -lhdf5

ifeq ($(THEHOST),jureca) # specific options for jureca cluster
# FLAGS += -DOPENBLAS_NOUSEOMP=1
 SOFLAGS += -fuse-ld=bfd
else
ifeq ($(THEHOST),cougar) # specific options for cougar cluster
 LIBS += -llapack
 PYTHONFLAGS = -I/opt/anaconda/include/python2.7 -L/opt/boost/1.58.0/lib -lboost_python #-lpython 
 ifeq ($(WITHHOARD),on)
  LIBS += -L/opt/hoard/3.12/lib -lhoard
 endif
else  # default options. assumes boost and python are set up nicely.
 LIBS += -llapack
 ifneq ($(PYTHON),off)
 ALL += pyIMSRG.so
 endif
endif
endif

all: $(ALL)

install:
	@if [ ! -d $(INSTDIR)/lib ] ; then \
	  mkdir $(INSTDIR)/lib; \
	fi
	@if [ ! -d $(INSTDIR)/include ] ; then \
	  mkdir $(INSTDIR)/include; \
	fi 
	ln -sf $(PWD)/libIMSRG.so $(INSTDIR)/lib/libIMSRG.so
	ln -sfT $(PWD)/armadillo $(INSTDIR)/include/armadillo
	@for x in *.hh; do \
	 echo linking $(PWD)/$$x  '=>'  $(INSTDIR)/include/$$x;\
	 ln -sf $(PWD)/$$x $(INSTDIR)/include/$$x; \
	done
	@printf "\n\nDone installing.\n\n"
	@echo '*********************************************************************'
	@echo '* Make sure libIMSRG.so is in your LIBRARY_PATH and LD_LIBRARY_PATH *'
	@echo '*********************************************************************'
	 

OBJ = ModelSpace.o TwoBodyME.o ThreeBodyME.o Operator.o  ReadWrite.o\
      HartreeFock.o imsrg_util.o Generator.o IMSRGSolver.o AngMom.o\
      boost_src/gzip.o boost_src/zlib.o  IMSRGProfiler.o 

mysrg: main.cc $(OBJ)
	$(CC) $^ -o $@ $(INCLUDE) $(LIBS) $(FLAGS) 

boost_src/gzip.o: boost_src/gzip.cpp
	$(CC) -c $^ -o $@ $(INCLUDE) $(LIBS) $(FLAGS) 

boost_src/zlib.o: boost_src/zlib.cpp
	$(CC) -c $^ -o $@ $(INCLUDE) $(LIBS) $(FLAGS) 


%.o: %.cc %.hh
	$(CC) -c $*.cc -o $@ $(INCLUDE) $(LIBS) $(FLAGS) 



libIMSRG.so: $(OBJ)
	$(CC) $^ -shared -o $@ $(SOFLAGS)

python: pyIMSRG.so

pyIMSRG.so: $(OBJ)  pyIMSRG.o
	$(CC) $^ -o $@ $(FLAGS) $(INCLUDE) $(LIBS) -shared $(PYTHONFLAGS)

pyIMSRG.o: pyIMSRG.cc
	$(CC) -c $^ -o $@ $(FLAGS) $(INCLUDE) $(LIBS) $(PYTHONFLAGS)

clean:
	rm -f *.o *.so boost_src/*.o
