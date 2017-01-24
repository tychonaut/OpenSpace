node {
	checkout scm
	sh 'git submodule update --init --recursive'
	sh 'mkdir build -p && cd build && python ../support/jenkins/buildAllModules.py && make'
}