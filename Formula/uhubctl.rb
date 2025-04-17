class Uhubctl < Formula
  desc "USB hub per-port power control"
  homepage "https://github.com/mvp/uhubctl"
  license "GPL-2.0-only"
  url "https://github.com/mvp/uhubctl/archive/refs/tags/v2.6.0.tar.gz"
  sha256 "56ca15ddf96d39ab0bf8ee12d3daca13cea45af01bcd5a9732ffcc01664fdfa2"
  head "https://github.com/mvp/uhubctl.git", branch: "master"

  depends_on "pkg-config" => :build
  depends_on "libusb"

  livecheck do
    url :stable
  end

  def install
    system "make"
    bin.install "uhubctl"
  end

  test do
    system bin/"uhubctl", "-v"
  end
end
