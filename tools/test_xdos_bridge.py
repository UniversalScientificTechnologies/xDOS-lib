"""Tests of the pure parts of xdos_bridge.py:  python3 -m unittest discover -s tools"""

import unittest

from xdos_bridge import checksum, frame_command, is_error, parse_response


class ChecksumTest(unittest.TestCase):
    def test_nmea_reference(self):
        # Classic NMEA 0183 example sentence with checksum 27 - the same
        # vector as in the C++ tests.
        self.assertEqual(checksum("GPGLL,5057.970,N,00146.110,E,142451,A"), 0x27)

    def test_frame_command(self):
        self.assertEqual(frame_command("EXIT"), b"!EXIT*00\r\n")
        self.assertEqual(frame_command("CONFIG,XDOS"), b"!CONFIG,XDOS*26\r\n")
        self.assertEqual(frame_command("I2CWR,5B,0800,10"), b"!I2CWR,5B,0800,10*6F\r\n")


class ResponseTest(unittest.TestCase):
    def test_valid(self):
        self.assertEqual(parse_response("#CONFIG_MODE,60*7C\r\n"), "CONFIG_MODE,60")
        self.assertEqual(parse_response("#CONFIG_EXIT*55"), "CONFIG_EXIT")

    def test_invalid(self):
        self.assertIsNone(parse_response("#CONFIG_EXIT*56"))    # wrong checksum
        self.assertIsNone(parse_response("#CONFIG_EXIT"))       # no checksum
        self.assertIsNone(parse_response("$STOP,1,2*00"))       # data line
        self.assertIsNone(parse_response("#Cvak1..."))          # debug line
        self.assertIsNone(parse_response(""))

    def test_is_error(self):
        self.assertTrue(is_error("I2C_ERR,NACK_ADDR"))
        self.assertTrue(is_error("CMD_ERR,CHECKSUM"))
        self.assertFalse(is_error("I2C_OK,0910"))


if __name__ == "__main__":
    unittest.main()
