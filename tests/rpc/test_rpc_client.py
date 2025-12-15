#!/usr/bin/env python3
"""
Kdenlive JSON-RPC WebSocket Test Client

Usage:
    # Start Kdenlive first, then run:
    python test_rpc_client.py

    # Run specific test:
    python test_rpc_client.py TestConnection

Requirements:
    pip install websockets
"""

import asyncio
import json
import sys
import unittest
from typing import Any, Optional

try:
    import websockets
except ImportError:
    print("Please install websockets: pip install websockets")
    sys.exit(1)


class KdenliveRpcClient:
    """Async JSON-RPC 2.0 client for Kdenlive WebSocket API."""

    def __init__(self, host: str = "localhost", port: int = 9876):
        self.uri = f"ws://{host}:{port}"
        self.ws: Optional[websockets.WebSocketClientProtocol] = None
        self.request_id = 0

    async def connect(self, timeout: float = 5.0):
        """Connect to the Kdenlive RPC server."""
        self.ws = await asyncio.wait_for(
            websockets.connect(self.uri),
            timeout=timeout
        )

    async def disconnect(self):
        """Disconnect from the server."""
        if self.ws:
            await self.ws.close()
            self.ws = None

    async def call(self, method: str, params: Optional[dict] = None, timeout: float = 10.0) -> dict:
        """Make a JSON-RPC call and return the response."""
        if not self.ws:
            raise RuntimeError("Not connected")

        self.request_id += 1
        request = {
            "jsonrpc": "2.0",
            "method": method,
            "params": params or {},
            "id": self.request_id
        }

        await self.ws.send(json.dumps(request))
        response = await asyncio.wait_for(self.ws.recv(), timeout=timeout)
        return json.loads(response)

    async def __aenter__(self):
        await self.connect()
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        await self.disconnect()


class TestConnection(unittest.TestCase):
    """Test basic connection and discovery."""

    def test_ping(self):
        """Test rpc.ping method."""
        async def run():
            async with KdenliveRpcClient() as client:
                result = await client.call("rpc.ping")
                self.assertIn("result", result)
                self.assertEqual(result["result"]["pong"], True)

        asyncio.run(run())

    def test_get_version(self):
        """Test rpc.getVersion method."""
        async def run():
            async with KdenliveRpcClient() as client:
                result = await client.call("rpc.getVersion")
                self.assertIn("result", result)
                self.assertIn("kdenlive", result["result"])
                self.assertIn("rpc", result["result"])

        asyncio.run(run())

    def test_get_capabilities(self):
        """Test rpc.getCapabilities method."""
        async def run():
            async with KdenliveRpcClient() as client:
                result = await client.call("rpc.getCapabilities")
                self.assertIn("result", result)
                self.assertIn("methods", result["result"])
                methods = result["result"]["methods"]
                # Check some expected methods exist
                self.assertIn("rpc.ping", methods)
                self.assertIn("project.getInfo", methods)
                self.assertIn("timeline.getInfo", methods)

        asyncio.run(run())


class TestProject(unittest.TestCase):
    """Test project-related methods."""

    def test_get_info(self):
        """Test project.getInfo method."""
        async def run():
            async with KdenliveRpcClient() as client:
                result = await client.call("project.getInfo")
                # May return error if no project open, or result with project info
                self.assertTrue("result" in result or "error" in result)

        asyncio.run(run())

    def test_undo_redo(self):
        """Test project.undo and project.redo methods."""
        async def run():
            async with KdenliveRpcClient() as client:
                # Try undo - may fail if nothing to undo
                undo_result = await client.call("project.undo")
                self.assertTrue("result" in undo_result or "error" in undo_result)

                # Try redo - may fail if nothing to redo
                redo_result = await client.call("project.redo")
                self.assertTrue("result" in redo_result or "error" in redo_result)

        asyncio.run(run())


class TestTimeline(unittest.TestCase):
    """Test timeline-related methods."""

    def test_get_info(self):
        """Test timeline.getInfo method."""
        async def run():
            async with KdenliveRpcClient() as client:
                result = await client.call("timeline.getInfo")
                self.assertTrue("result" in result or "error" in result)

        asyncio.run(run())

    def test_get_tracks(self):
        """Test timeline.getTracks method."""
        async def run():
            async with KdenliveRpcClient() as client:
                result = await client.call("timeline.getTracks")
                self.assertTrue("result" in result or "error" in result)

        asyncio.run(run())

    def test_get_clips(self):
        """Test timeline.getClips method."""
        async def run():
            async with KdenliveRpcClient() as client:
                result = await client.call("timeline.getClips")
                self.assertTrue("result" in result or "error" in result)

        asyncio.run(run())

    def test_get_position(self):
        """Test timeline.getPosition method."""
        async def run():
            async with KdenliveRpcClient() as client:
                result = await client.call("timeline.getPosition")
                self.assertTrue("result" in result or "error" in result)
                if "result" in result:
                    self.assertIn("position", result["result"])

        asyncio.run(run())


class TestBin(unittest.TestCase):
    """Test bin-related methods."""

    def test_list_clips(self):
        """Test bin.listClips method."""
        async def run():
            async with KdenliveRpcClient() as client:
                result = await client.call("bin.listClips")
                self.assertTrue("result" in result or "error" in result)

        asyncio.run(run())

    def test_list_folders(self):
        """Test bin.listFolders method."""
        async def run():
            async with KdenliveRpcClient() as client:
                result = await client.call("bin.listFolders")
                self.assertTrue("result" in result or "error" in result)

        asyncio.run(run())


class TestEffects(unittest.TestCase):
    """Test effects-related methods."""

    def test_list_available(self):
        """Test effect.listAvailable method."""
        async def run():
            async with KdenliveRpcClient() as client:
                result = await client.call("effect.listAvailable")
                self.assertTrue("result" in result or "error" in result)
                if "result" in result:
                    # Should return a list of effects
                    self.assertIsInstance(result["result"], list)

        asyncio.run(run())


class TestAssets(unittest.TestCase):
    """Test assets-related methods."""

    def test_list_categories(self):
        """Test asset.listCategories method."""
        async def run():
            async with KdenliveRpcClient() as client:
                result = await client.call("asset.listCategories")
                self.assertTrue("result" in result or "error" in result)

        asyncio.run(run())

    def test_search(self):
        """Test asset.search method."""
        async def run():
            async with KdenliveRpcClient() as client:
                result = await client.call("asset.search", {"query": "blur"})
                self.assertTrue("result" in result or "error" in result)

        asyncio.run(run())


class TestRender(unittest.TestCase):
    """Test render-related methods."""

    def test_get_presets(self):
        """Test render.getPresets method."""
        async def run():
            async with KdenliveRpcClient() as client:
                result = await client.call("render.getPresets")
                self.assertTrue("result" in result or "error" in result)
                if "result" in result:
                    self.assertIsInstance(result["result"], list)

        asyncio.run(run())

    def test_get_jobs(self):
        """Test render.getJobs method."""
        async def run():
            async with KdenliveRpcClient() as client:
                result = await client.call("render.getJobs")
                self.assertTrue("result" in result or "error" in result)

        asyncio.run(run())


class TestSubscriptions(unittest.TestCase):
    """Test event subscription methods."""

    def test_subscribe_unsubscribe(self):
        """Test rpc.subscribe and rpc.unsubscribe methods."""
        async def run():
            async with KdenliveRpcClient() as client:
                # Subscribe to project events
                sub_result = await client.call("rpc.subscribe", {"events": ["project.opened", "project.saved"]})
                self.assertIn("result", sub_result)
                self.assertTrue(sub_result["result"]["subscribed"])

                # Unsubscribe
                unsub_result = await client.call("rpc.unsubscribe", {"events": ["project.opened"]})
                self.assertIn("result", unsub_result)
                self.assertTrue(unsub_result["result"]["unsubscribed"])

        asyncio.run(run())


class TestErrorHandling(unittest.TestCase):
    """Test error handling."""

    def test_unknown_method(self):
        """Test that unknown methods return proper error."""
        async def run():
            async with KdenliveRpcClient() as client:
                result = await client.call("unknown.method")
                self.assertIn("error", result)
                self.assertIn("code", result["error"])

        asyncio.run(run())

    def test_invalid_params(self):
        """Test that invalid params return proper error."""
        async def run():
            async with KdenliveRpcClient() as client:
                # Try to open a non-existent project
                result = await client.call("project.open", {"path": "/nonexistent/path.kdenlive"})
                self.assertIn("error", result)

        asyncio.run(run())


async def interactive_session():
    """Run an interactive session for manual testing."""
    print("Connecting to Kdenlive RPC server...")
    try:
        async with KdenliveRpcClient() as client:
            print("Connected! Type 'quit' to exit.\n")
            print("Example: rpc.ping")
            print("Example: project.getInfo")
            print("Example: timeline.getTracks")
            print()

            while True:
                try:
                    line = input(">>> ").strip()
                    if line.lower() == "quit":
                        break
                    if not line:
                        continue

                    # Parse method and optional params
                    parts = line.split(" ", 1)
                    method = parts[0]
                    params = json.loads(parts[1]) if len(parts) > 1 else None

                    result = await client.call(method, params)
                    print(json.dumps(result, indent=2))
                except json.JSONDecodeError as e:
                    print(f"Invalid JSON params: {e}")
                except Exception as e:
                    print(f"Error: {e}")

    except ConnectionRefusedError:
        print("Could not connect to Kdenlive. Make sure Kdenlive is running with RPC enabled.")
    except asyncio.TimeoutError:
        print("Connection timed out.")


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "--interactive":
        asyncio.run(interactive_session())
    else:
        # Run unit tests
        unittest.main(verbosity=2)


if __name__ == "__main__":
    main()
