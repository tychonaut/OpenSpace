node {
	checkout scm
	sh 'git submodule update --init --recursive'
	sh 'ls'
	sh 'mkdir build -p'
	sh 'cd build'
	sh 'python ../support/jenkins/buildAllModules.py'
	sh 'make'
}