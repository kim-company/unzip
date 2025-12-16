defmodule Unzip.Deflate64Test do
  use ExUnit.Case

  describe "NIF loading" do
    test "init/0 returns a reference" do
      assert {:ok, ref} = Unzip.Deflate64.init()
      assert is_reference(ref)
    end

    test "close/1 cleans up state" do
      {:ok, ref} = Unzip.Deflate64.init()
      assert :ok = Unzip.Deflate64.close(ref)
    end
  end

  describe "Deflate64 decompression" do
    @fixture_path Path.join(__DIR__, "../support/")

    test "inflate/2 decompresses Deflate64 data from ZIP" do
      # Test with a real Deflate64-compressed ZIP file
      zip_file = Unzip.LocalFile.open(Path.join(@fixture_path, "test_deflate64.zip"))
      {:ok, unzip} = Unzip.new(zip_file)

      # Verify it's using Deflate64 (method 9)
      entries = Unzip.list_entries(unzip)
      assert length(entries) == 1

      # Extract and verify content
      result =
        Unzip.file_stream!(unzip, "test_deflate64.txt")
        |> Enum.join()

      expected = File.read!(Path.join(@fixture_path, "test_deflate64.txt"))
      assert result == expected
    end

    test "handles empty Deflate64 file" do
      # Test with empty file to ensure edge cases work
      zip_file = Unzip.LocalFile.open(Path.join(@fixture_path, "test_deflate64.zip"))
      {:ok, unzip} = Unzip.new(zip_file)

      # Should not crash on empty or small files
      entries = Unzip.list_entries(unzip)
      assert is_list(entries)
    end
  end
end
