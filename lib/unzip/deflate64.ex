defmodule Unzip.Deflate64 do
  @moduledoc """
  NIF wrapper for Deflate64 (compression method 9) decompression.
  Uses infback9 from zlib contrib for streaming decompression.
  """

  @on_load :load_nif

  @doc false
  def load_nif do
    nif_path =
      :code.priv_dir(:unzip)
      |> to_string()
      |> Path.join("deflate64_nif")

    case :erlang.load_nif(to_charlist(nif_path), 0) do
      :ok -> :ok
      {:error, {:reload, _}} -> :ok
      {:error, reason} -> {:error, reason}
    end
  end

  @doc """
  Initialize a new Deflate64 decompression state.

  Returns `{:ok, state}` on success or `{:error, reason}` on failure.
  """
  @spec init() :: {:ok, reference()} | {:error, term()}
  def init do
    nif_init()
  end

  @doc """
  Decompress a chunk of Deflate64-compressed data.

  Takes a state returned by `init/0` and a binary of compressed data.
  Returns `{:ok, decompressed_data}` or `{:error, reason}`.

  The state is stateful and maintains the decompression context across
  multiple calls, allowing for streaming decompression.
  """
  @spec inflate(reference(), binary()) :: {:ok, binary()} | {:error, term()}
  def inflate(state, data) when is_reference(state) and is_binary(data) do
    nif_inflate(state, data)
  end

  @doc """
  Clean up a Deflate64 decompression state.

  Should be called when decompression is complete to free resources.
  """
  @spec close(reference()) :: :ok
  def close(state) when is_reference(state) do
    nif_end(state)
  end

  # NIF stubs - these will be replaced by the actual NIF functions

  defp nif_init do
    :erlang.nif_error(:nif_not_loaded)
  end

  defp nif_inflate(_state, _data) do
    :erlang.nif_error(:nif_not_loaded)
  end

  defp nif_end(_state) do
    :erlang.nif_error(:nif_not_loaded)
  end
end
