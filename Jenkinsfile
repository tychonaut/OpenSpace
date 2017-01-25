stage('Build') {
	parallel linux: {
		node('linux') {
			checkout scm
			sh 'git submodule update --init --recursive'
			sh 'mkdir build && cd build && python ../support/jenkins/buildAllModules.py && make'
		}
	},
	windows: {
		node('windows') {
			checkout scm
			sh 'git submodule update --init --recursive'
			sh 'mkdir build && cd build && python ..\support\jenkins\buildAllModules.py && make'
		}
	}
	mac: {
		node('mac') {
			checkout scm
			sh 'git submodule update --init --recursive'
			sh 'mkdir build && cd build && python ../support/jenkins/buildAllModules.py && make'
		}
	}
}