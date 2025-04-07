pipeline {
    agent any

    options {
        githubProjectProperty(
            projectUrlStr: 'https://github.com/Durham-Adaptive-Optics/daoBase'
        )
    }

    environment {
        GITHUB_TOKEN = credentials('github-token')
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
            node {
                cleanWs()
            }
        }
        success {
            node {
                echo 'Build completed successfully!'
                step([
                    $class: 'GitHubCommitStatusSetter',
                    reposSource: [$class: 'ManuallyEnteredRepositorySource', url: 'https://github.com/Durham-Adaptive-Optics/daoBase'],
                    commitShaSource: [$class: 'BuildDataRevisionShaSource'],
                    contextSource: [$class: 'ManuallyEnteredCommitContextSource', context: 'Jenkins build'],
                    statusResultSource: [$class: 'ConditionalStatusResultSource', results: [[$class: 'AnyBuildResult', message: 'Build successful', state: 'SUCCESS']]],
                    errorHandlers: [[$class: 'ChangingBuildStatusErrorHandler', result: 'UNSTABLE']]
                ])
            }
        }
        failure {
            node {
                echo 'Build failed!'
                step([
                    $class: 'GitHubCommitStatusSetter',
                    reposSource: [$class: 'ManuallyEnteredRepositorySource', url: 'https://github.com/Durham-Adaptive-Optics/daoBase'],
                    commitShaSource: [$class: 'BuildDataRevisionShaSource'],
                    contextSource: [$class: 'ManuallyEnteredCommitContextSource', context: 'Jenkins build'],
                    statusResultSource: [$class: 'ConditionalStatusResultSource', results: [[$class: 'AnyBuildResult', message: 'Build failed', state: 'FAILURE']]],
                    errorHandlers: [[$class: 'ChangingBuildStatusErrorHandler', result: 'UNSTABLE']]
                ])
            }
        }
    }
}
