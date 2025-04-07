pipeline {
    agent any

    stages {
        stage('Checkout') {
            steps {
                checkout scm
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
        }
        failure {
            echo 'Build failed!'
        }
    }
}
