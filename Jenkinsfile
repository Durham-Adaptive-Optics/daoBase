pipeline {
    agent any

    triggers {
        pollSCM('H/5 * * * *') // Check every 5 minutes
    }

    stages {
        stage('Checkout') {
            steps {
                checkout scm
                // Set pending status
                githubNotify context: 'CI/Build', description: 'Build in progress', status: 'PENDING'
            }
        }
        
        stage('Prepare') {
            steps {
                // Download waf
                sh 'wget https://waf.io/waf-2.0.24'
                sh 'mv waf-2.0.24 waf'
                sh 'chmod +x waf'
            }
        }

        stage('Debug') {
            steps {
                sh 'git rev-parse HEAD'
                echo "Building commit: ${env.GIT_COMMIT}"
            }
        }

        stage('Build') {
            steps {
                sh '''
                    # Configure and build using waf
                    ./waf configure
                    ./waf build
                '''
            }
        }
    }

    post {
        always {
            cleanWs()
        }
        success {
            echo 'Build completed successfully!'
            githubNotify context: 'CI/Build', description: 'Build succeeded', status: 'SUCCESS'
        }
        failure {
            echo 'Build failed!'
            githubNotify context: 'CI/Build', description: 'Build failed', status: 'FAILURE'
        }
    }
}