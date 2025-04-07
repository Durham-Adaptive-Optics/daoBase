pipeline {
    agent any

    options {
        githubProjectProperty(
            projectUrlStr: 'https://github.com/Durham-Adaptive-Optics/daoBase'
        )
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
        success {
            echo 'Build completed successfully!'
            githubCommitStatus(
                repoUrl: 'https://github.com/Durham-Adaptive-Optics/daoBase',
                message: 'Build successful',
                state: 'SUCCESS'
            )
        }
        failure {
            echo 'Build failed!'
            githubCommitStatus(
                repoUrl: 'https://github.com/Durham-Adaptive-Optics/daoBase',
                message: 'Build failed',
                state: 'FAILURE'
            )
        }
        always {
            cleanWs()
        }
    }
}

// Add this at the top level, outside the pipeline
def githubCommitStatus(Map args) {
    step([
        $class: 'GitHubCommitStatusSetter',
        reposSource: [$class: 'ManuallyEnteredRepositorySource', url: args.repoUrl],
        commitShaSource: [$class: 'BuildDataRevisionShaSource'],
        contextSource: [$class: 'ManuallyEnteredCommitContextSource', context: 'Jenkins build'],
        statusResultSource: [$class: 'ConditionalStatusResultSource', results: [[$class: 'AnyBuildResult', message: args.message, state: args.state]]],
        errorHandlers: [[$class: 'ChangingBuildStatusErrorHandler', result: 'UNSTABLE']]
    ])
}
