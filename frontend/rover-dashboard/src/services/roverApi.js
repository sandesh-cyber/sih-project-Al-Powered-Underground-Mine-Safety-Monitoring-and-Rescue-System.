const ROVER_BASE_URL = "http://192.168.4.1";

/**
 * Expected data fields from the Rover-12.1 /data endpoint:
 *
 * - rover: Rover internal telemetry / status object
 * - riskScore: Calculated environment/safety risk score
 * - riskLevel: Risk category level (e.g. SAFE, WARNING, CRITICAL)
 * - riskReason: Explanation of primary risk factor
 * - sensorConfidence: Reliability rating of active sensors
 * - minerID: Identifier of assigned miner / smart helmet
 * - helmetLink: Connection status flag to the miner helmet
 * - lastPacketAgeMs: Elapsed time in milliseconds since last packet
 * - helmetMQ4Change: Gas change rate detected by helmet MQ4 sensor
 * - roverMQ4Change: Gas change rate detected by rover MQ4 sensor
 * - water: Water level / detection indicator
 * - smoke: Smoke detection indicator
 * - distanceCm: Measured obstacle distance in centimeters
 * - distanceValid: Flag indicating if distance reading is valid
 * - helmetSOS: Emergency SOS alert triggered by miner helmet
 * - helmetFall: Fall alert triggered by miner helmet
 * - packetsReceived: Total count of received telemetry packets
 * - missedPackets: Count of dropped/lost telemetry packets
 * - packetDeliveryPercent: Percentage of successful packet deliveries
 */

/**
 * Fetches telemetry and sensor data from the ESP32 Rover.
 * Sends a GET request to http://192.168.4.1/data
 *
 * @returns {Promise<Object>} The parsed JSON data from the Rover.
 * @throws {Error} If the HTTP request fails or the response status is not OK.
 */
export async function getRoverData() {
  const response = await fetch(`${ROVER_BASE_URL}/data`);

  if (!response.ok) {
    throw new Error(
      `Failed to fetch rover data: HTTP ${response.status} ${response.statusText}`
    );
  }

  const data = await response.json();
  return data;
}

/**
 * Checks whether the Rover is currently reachable over Wi-Fi.
 *
 * @returns {Promise<boolean>} True if communication succeeds, false otherwise.
 */
export async function checkRoverConnection() {
  try {
    await getRoverData();
    return true;
  } catch {
    return false;
  }
}
