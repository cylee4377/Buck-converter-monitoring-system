import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import './index.css'
import App from './App.jsx'
import { Amplify } from 'aws-amplify'
import outputs from '../amplify_outputs.json'

// AWS Amplify Gen2 초기화 (AppSync GraphQL 엔드포인트, API Key 등 자동 설정)
Amplify.configure(outputs)

createRoot(document.getElementById('root')).render(
  <StrictMode>
    <App />
  </StrictMode>,
)
