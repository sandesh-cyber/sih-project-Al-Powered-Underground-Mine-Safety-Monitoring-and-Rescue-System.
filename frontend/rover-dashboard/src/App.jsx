import { useState, useEffect } from 'react'
import { getRoverData } from './services/roverApi'
import Dashboard from './pages/Dashboard'

function App() {
  const [roverData, setRoverData] = useState(null)
  const [isConnected, setIsConnected] = useState(false)

  useEffect(() => {
    let isMounted = true

    const fetchData = async () => {
      try {
        const data = await getRoverData()
        if (isMounted) {
          setRoverData(data)
          setIsConnected(true)
        }
      } catch {
        if (isMounted) {
          setIsConnected(false)
        }
      }
    }

    // Initial fetch on mount
    fetchData()

    // 1-second interval to refresh Rover data
    const intervalId = setInterval(fetchData, 1000)

    return () => {
      isMounted = false
      clearInterval(intervalId)
    }
  }, [])

  return <Dashboard roverData={roverData} isConnected={isConnected} />
}

export default App
