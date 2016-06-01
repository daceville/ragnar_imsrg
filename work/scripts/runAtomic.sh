#!/bin/bash

exe=../compiled/Atomic

#vnn=/itch/exch/me2j/chi2b_srg0625_eMax14_lMax10_hwHO024.me2j.gz
v3n=none

start=1
stop=1

for ((A=$start;A<=$stop;A++)); do

hw=54.422
valence_space=s-shell
reference=H1
smax=200
emax=5
e3max=12
method=HF
basis=oscillator
omega_norm_max=0.25
#file3='file3e1max=12 file3e2max=28 file3e3max=12'

Operators=KineticEnergy,InverseR
scratch=

$exe 2bme=${vnn} 3bme=${v3n} emax=${emax} e3max=${e3max} method=${method} valence_space=${valence_space} hw=${hw} smax=${smax} ${file3} omega_norm_max=${omega_norm_max} reference=${reference} Operators=${Operators} scratch=${scratch} A=${A} use_brueckner_bch=false

done

