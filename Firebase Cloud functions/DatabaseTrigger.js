// The Cloud Functions for Firebase SDK to create Cloud Functions and setup triggers.
const functions = require('firebase-functions');

// The Firebase Admin SDK to access the Firebase Realtime Database.
const admin = require('firebase-admin');

admin.initializeApp(functions.config().firebase);

// Update the timestamp key in the newly added rideObject -- Saved ridesThrough Arduino WEMOS
exports.onKMTrackerRide = functions.database.ref('/KMTracker/Rides/{pushId}').onCreate(event => {
  console.log("New Ride created by KMTracker")
  console.log("Updating timestamp...");
  var snapshot = event.data.ref;
  var utc_timestamp = getCurrentTimestamp();
  return snapshot.update({
    "date": utc_timestamp
  });
});

function getCurrentTimestamp(){
  var now = new Date();
  return Date.UTC(now.getUTCFullYear(),now.getUTCMonth(), now.getUTCDate() ,
      (now.getUTCHours()), now.getUTCMinutes(), now.getUTCSeconds(), now.getUTCMilliseconds());
}
