#!/usr/bin/env bash
scp ~/rpmbuild/RPMS/x86_64/oregano*.rpm http://drone.beerbach.me:/srv/rpms/oregano
scp ~/rpmbuild/RPMS/x86_64/oregano*.rpm http://drone.beerbach.me:/srv/repo/
