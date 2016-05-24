#!/bin/bash

exe=../compiled/Atomic

vnn=../../input/chi2b_srg0800_eMax12_lMax10_hwHO020.me2j.gz
v3n=none

for ((A=1;A<=1;A++)); do

hw=20
valence_space=spd-shell
reference=O$A
smax=200
emax=4
e3max=12
method=HF
omega_norm_max=0.25
file3='file3e1max=12 file3e2max=28 file3e3max=12'

Operators=
scratch=SCRATCH

$exe 2bme=${vnn} 3bme=${v3n} emax=${emax} e3max=${e3max} method=${method} valence_space=${valence_space} hw=${hw} smax=${smax} ${file3} omega_norm_max=${omega_norm_max} reference=${reference} Operators=${Operators} scratch=${scratch} A=${A} use_brueckner_bch=true

done

