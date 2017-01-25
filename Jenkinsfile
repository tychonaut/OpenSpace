stage('Build') {
	parallel linux: {
		node('linux') {
			checkout scm
			sh 'git submodule update --init --recursive'
			sh 'mkdir -p build && cd build && python ../support/jenkins/buildAllModules.py && make'
		}
	},
	osx: {
		node('osx') {
			checkout scm
			sh 'git submodule update --init --recursive'
			sh 'mkdir -p build'
			sh 'cd build && cmake -DGHOUL_USE_DEVIL=OFF .. && make'
		}
	}
}	