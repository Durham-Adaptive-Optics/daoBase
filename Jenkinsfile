pipeline {
    agent any

    environment {
        // This will securely access your GitHub token from Jenkins credentials
        GITHUB_TOKEN = credentials('github-token-credential-id')
        GIT_COMMIT_SHA = sh(script: 'git rev-parse HEAD', returnStdout: true).trim()

    }

    triggers {
        pollSCM('H/5 * * * *') // Check every 5 minutes
    }

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
            updateGithubStatus('success', 'The build succeeded!')
        }
        failure {
            echo 'Build failed!'
            updateGithubStatus('failure', 'The build failed!')
        }
    }
}

// Function to update GitHub commit status
def updateGithubStatus(String state, String description) {
    sh """
        curl -L \
          -X POST \
          -H "Accept: application/vnd.github+json" \
          -H "Authorization: Bearer ${GITHUB_TOKEN}" \
          -H "X-GitHub-Api-Version: 2022-11-28" \
          https://api.github.com/repos/Durham-Adaptive-Optics/daoBase/statuses/${GIT_COMMIT_SHA} \
          -d '{"state":"${state}","context":"Jenkins CI","description":"${description}"}'
    """
}