class Uhubctl < Formula
  desc "control USB hubs powering per-port"
  homepage "https://github.com/mvp/uhubctl"
  head "https://github.com/mvp/uhubctl.git"

  depends_on "libusb"

  def install
    system "make"
    bin.install "uhubctl"
  end

  test do
    system "#{bin}/uhubctl", "-v"
  end
end
